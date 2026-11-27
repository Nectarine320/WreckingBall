#include "AI/WB_SoccerAIController_Base.h"

#include "Debug/WB_DebugTextSubsystem.h"
#include "AI/WB_SoccerAIProfile.h"
#include "Data/WB_DataReadLibrary.h"
#include "GameFramework/GameModeBase.h"
#include "AI/Team/WB_TeamCoordinatorComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Impact/Destructible/Door/WB_HingedDoorComponent.h"
#include "Match/WB_GoalkeeperZoneComponent.h"
#include "Match/WB_TeamGoalComponent.h"
#include "Match/WB_TeamLibrary.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Player/Command/WB_PlayerCommandComponent.h"
#include "Player/Pass/WB_PassProfile.h"
#include "Player/Dive/WB_PlayerDiveComponent.h"
#include "Player/Hit/WB_PlayerHitComponent.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"
#include "AI/WB_SoccerAITags.h"
#include "Match/Roster/WB_TeamRosterComponent.h"

namespace
{
	/** Rotates a flat direction so it stays within HalfAngle degrees of Axis. */
	FVector ClampDirectionToCone(const FVector& Direction, const FVector& Axis, float HalfAngle)
	{
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Axis), -1.f, 1.f)));
		if (Angle <= HalfAngle)
		{
			return Direction;
		}
		const float Side = FVector::CrossProduct(Axis, Direction).Z >= 0.f ? 1.f : -1.f;
		return Axis.RotateAngleAxis(Side * HalfAngle, FVector::UpVector);
	}

	bool IsHoldingBall(const UWB_PlayerStateComponent* State)
	{
		return State && State->CurrentHolding.MatchesTag(WBTags::Player_Holding_Ball);
	}

	/** True when the debug filter of Profile allows an action: the filter is off, or its switch bSwitch is on. */
	bool IsActionAllowed(const UWB_SoccerAIProfile* Profile, bool bSwitch)
	{
		return !Profile || !Profile->bUseFeatureFilter || bSwitch;
	}
}

bool AWB_SoccerAIController_Base::IsIntentAllowed(EWB_SoccerAIIntent Intent) const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile || !Profile->bUseFeatureFilter)
	{
		return true;
	}
	switch (Intent)
	{
	case EWB_SoccerAIIntent::SeekBall:     return Profile->bAllowSeekBall;
	case EWB_SoccerAIIntent::ChaseCarrier: return Profile->bAllowChaseCarrier;
	case EWB_SoccerAIIntent::Support:      return Profile->bAllowSupport;
	case EWB_SoccerAIIntent::Attack:       return Profile->bAllowAttack;
	case EWB_SoccerAIIntent::Press:        return Profile->bAllowPress;
	case EWB_SoccerAIIntent::Defend:       return Profile->bAllowDefend;
	case EWB_SoccerAIIntent::GuardGoal:    return Profile->bAllowGuardGoal;
	case EWB_SoccerAIIntent::LastMan:      return Profile->bAllowLastMan;
	case EWB_SoccerAIIntent::SupportOpen:  return Profile->bAllowSupportOpen;
	case EWB_SoccerAIIntent::ReceivePass:  return Profile->bAllowReceivePass;
	default:                             return true;
	}
}

AWB_SoccerAIController_Base::AWB_SoccerAIController_Base()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
	// The bot turns its pawn itself (SetFacingDirection); the controller rotation follows the pawn.
	bSetControlRotationFromPawnOrientation = true;
}

const UWB_SoccerAIProfile* AWB_SoccerAIController_Base::GetActiveProfile() const
{
	return AIProfile.Get();
}

int32 AWB_SoccerAIController_Base::GetTeamIndex() const
{
	return UWB_TeamLibrary::GetTeamOf(GetPawn());
}

float AWB_SoccerAIController_Base::GetDashCooldown() const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile)
	{
		return 0.f;
	}
	const int32 Team = GetTeamIndex();
	const UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!Profile->bUseRubberBandDash || Profile->RubberBandDashCooldowns.IsEmpty() || !GameMode || (Team != 0 && Team != 1))
	{
		return Profile->DashCooldown;
	}

	float Score0 = 0.f;
	float Score1 = 0.f;
	if (!UWB_DataReadLibrary::ReadNumber(GameMode, Profile->ScoreTeam0Variable, Score0) || !UWB_DataReadLibrary::ReadNumber(GameMode, Profile->ScoreTeam1Variable, Score1))
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogTemp, Warning, TEXT("%s: rubber band on, but the game mode %s has no score variables '%s' / '%s'. The bots use DashCooldown."),
				*GetName(), *GetNameSafe(GameMode), *Profile->ScoreTeam0Variable.ToString(), *Profile->ScoreTeam1Variable.ToString());
		}
		return Profile->DashCooldown;
	}

	// Goals behind the other team. Tied or leading: the base cooldown.
	const int32 Gap = FMath::RoundToInt(Team == 0 ? Score1 - Score0 : Score0 - Score1);
	if (Gap <= 0)
	{
		return Profile->DashCooldown;
	}
	const int32 Index = FMath::Min(Gap, Profile->RubberBandDashCooldowns.Num()) - 1;
	return FMath::Max(Profile->RubberBandDashCooldowns[Index], 0.f);
}

void AWB_SoccerAIController_Base::ResetDashCooldown()
{
	NextDashTime = 0.0;
}

void AWB_SoccerAIController_Base::SetBrainPaused(bool bPaused)
{
	bBrainPaused = bPaused;
	if (bPaused)
	{
		if (Commands)
		{
			Commands->StopMoving();
		}
		ResetBrain();
	}
}

FGenericTeamId AWB_SoccerAIController_Base::GetGenericTeamId() const
{
	const int32 Team = GetTeamIndex();
	return Team >= 0 ? FGenericTeamId(static_cast<uint8>(Team)) : FGenericTeamId::NoTeam;
}

bool AWB_SoccerAIController_Base::IsOpponent(const APawn* Other) const
{
	if (!Other || Other == GetPawn())
	{
		return false;
	}
	const int32 MyTeam = GetTeamIndex();
	const int32 OtherTeam = UWB_TeamLibrary::GetTeamOf(Other);
	return MyTeam < 0 || OtherTeam < 0 || OtherTeam != MyTeam;
}

UWB_TeamGoalComponent* AWB_SoccerAIController_Base::GetGoalToAttack() const
{
	const APawn* Me = GetPawn();
	return Me ? UWB_TeamGoalComponent::FindGoalToAttack(this, GetTeamIndex(), Me->GetActorLocation()) : nullptr;
}

UWB_TeamGoalComponent* AWB_SoccerAIController_Base::GetGoalToDefend() const
{
	return UWB_TeamGoalComponent::FindGoalDefendedBy(this, GetTeamIndex());
}

UWB_GoalkeeperZoneComponent* AWB_SoccerAIController_Base::GetOwnGoalkeeperZone() const
{
	return UWB_GoalkeeperZoneComponent::FindZoneForTeam(this, GetTeamIndex());
}

float AWB_SoccerAIController_Base::GetPerimeterDistance(const FVector& Direction) const
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone();
	return (OwnGoal && Zone) ? Zone->GetExitDistance(OwnGoal->GetAimLocation(), Direction) : 0.f;
}

void AWB_SoccerAIController_Base::SetTeamRole(EWB_SoccerRole InRole, EWB_TeamSituation InSituation, APawn* InPressTarget)
{
	TeamSituation = InSituation;
	// A forced role comes before the coordinator (ChooseForcedIntent sets CurrentRole itself).
	if (GetForcedRole().IsValid())
	{
		PressTarget.Reset();
		return;
	}
	CurrentRole = InRole;
	PressTarget = InPressTarget;
}

FGameplayTag AWB_SoccerAIController_Base::GetForcedRole() const
{
	const UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this);
	return Roster ? Roster->GetForcedRoleOf(GetPawn()) : FGameplayTag();
}

bool AWB_SoccerAIController_Base::ChooseForcedIntent(const FGameplayTag& ForcedRole, EWB_SoccerAIIntent& OutIntent)
{
	const bool bGoalkeeper = ForcedRole.MatchesTagExact(WBTags::AI_ForcedRole_Goalkeeper);
	const bool bAggressive = ForcedRole.MatchesTagExact(WBTags::AI_ForcedRole_Aggressive);
	if (!bGoalkeeper && !bAggressive)
	{
		return false;
	}
	CurrentRole = bGoalkeeper ? EWB_SoccerRole::Goalkeeper : EWB_SoccerRole::Attacker;
	PressTarget.Reset();

	if (IsHoldingBall(PlayerStateComp))
	{
		OutIntent = bGoalkeeper ? EWB_SoccerAIIntent::ClearBall : EWB_SoccerAIIntent::KeepAway;
		return true;
	}
	if (PlayerStateComp && PlayerStateComp->IsHoldingAnything())
	{
		OutIntent = EWB_SoccerAIIntent::Attack;
		return true;
	}
	if (RefreshIncomingPass() && IsIntentAllowed(EWB_SoccerAIIntent::ReceivePass))
	{
		OutIntent = EWB_SoccerAIIntent::ReceivePass;
		return true;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (bGoalkeeper)
	{
		// Never leaves its goal (no LastMan): guards it, and claims a free ball inside its zone.
		OutIntent = EWB_SoccerAIIntent::GuardGoal;
		if (Profile->bGoalkeeperClaimsBallInZone && CachedBall.IsValid() && !FindBallCarrier() && IsIntentAllowed(EWB_SoccerAIIntent::SeekBall))
		{
			const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone();
			if (Zone && Zone->IsLocationInside(CachedBall->GetActorLocation()))
			{
				OutIntent = EWB_SoccerAIIntent::SeekBall;
			}
		}
		return true;
	}

	// Aggressive: the free ball, else the opponent carrier (Dash to make it drop the ball).
	if (const APawn* Carrier = FindBallCarrier())
	{
		OutIntent = IsOpponent(Carrier) ? EWB_SoccerAIIntent::ChaseCarrier : EWB_SoccerAIIntent::Support;
		return true;
	}
	OutIntent = CachedBall.IsValid() ? EWB_SoccerAIIntent::SeekBall : EWB_SoccerAIIntent::Idle;
	return true;
}

void AWB_SoccerAIController_Base::TickKeepAway(ACharacter* Me, float DeltaSeconds)
{
	TArray<FVector> Opponents;
	GatherOpponentLocations(Opponents);
	const FVector MyLocation = Me->GetActorLocation();
	const FVector* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	for (const FVector& Location : Opponents)
	{
		const double DistSq = FVector::DistSquared2D(Location, MyLocation);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = &Location;
		}
	}
	if (!Nearest)
	{
		Commands->StopMoving();
		return;
	}
	const float KeepDistance = GetActiveProfile()->AggressiveKeepAwayDistance;
	const FVector Away = (MyLocation - *Nearest).GetSafeNormal2D();
	if (FMath::Sqrt(NearestDistSq) < KeepDistance && !Away.IsNearlyZero())
	{
		// Run to the point KeepDistance away from the opponent, on the far side (the path goes around the walls).
		MoveTowardLocation(Me, *Nearest + Away * KeepDistance);
		return;
	}
	Commands->StopMoving();
	TurnToward(Me, Away.IsNearlyZero() ? Me->GetActorForwardVector() : -Away, DeltaSeconds);
}

void AWB_SoccerAIController_Base::TickClearBall(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* Goal = GetGoalToAttack();
	if (!Goal)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	Commands->StopMoving();
	// Same aim as a shot (toward the opponent goal, ShootAimJitter), then a short charged kick: TickCharge releases it.
	const float Remaining = TurnToward(Me, GetShotDirection(Me, Goal), DeltaSeconds);
	if (Remaining <= Profile->ShootAimTolerance && IsActionAllowed(Profile, Profile->bAllowShoot))
	{
		ChargeTarget = Profile->GoalkeeperClearChargeTime;
		ChargeElapsed = 0.f;
		bCharging = true;
		Commands->KickPress();
	}
}

APawn* AWB_SoccerAIController_Base::FindBallCarrier() const
{
	// Asked several times per Tick (intent, roles, support...): the search runs once per frame.
	if (CarrierFrame == GFrameCounter)
	{
		return CachedCarrier.Get();
	}
	CarrierFrame = GFrameCounter;
	CachedCarrier.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (IsHoldingBall(It->FindComponentByClass<UWB_PlayerStateComponent>()))
		{
			CachedCarrier = *It;
			break;
		}
	}
	return CachedCarrier.Get();
}

void AWB_SoccerAIController_Base::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	PlayerStateComp = InPawn ? InPawn->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	PlayerHit = InPawn ? InPawn->FindComponentByClass<UWB_PlayerHitComponent>() : nullptr;
	Commands = InPawn ? InPawn->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
	DiveComp = InPawn ? InPawn->FindComponentByClass<UWB_PlayerDiveComponent>() : nullptr;
	ResetBrain();

	if (!Commands)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: %s has no UWB_PlayerCommandComponent, the bot cannot act."), *GetName(), *GetNameSafe(InPawn));
	}
}

void AWB_SoccerAIController_Base::OnUnPossess()
{
	if (Commands)
	{
		Commands->StopMoving();
	}
	// A stance started by the bot has no button to release it: end it before a human takes the character.
	if (DiveComp && DiveComp->IsDefending())
	{
		DiveComp->StopDefense();
	}
	PlayerStateComp = nullptr;
	PlayerHit = nullptr;
	Commands = nullptr;
	DiveComp = nullptr;
	// The role belongs to the character: the coordinator gives the new one at its next evaluation.
	CurrentRole = EWB_SoccerRole::None;
	PressTarget.Reset();
	ResetBrain();
	Super::OnUnPossess();
}

void AWB_SoccerAIController_Base::ResetBrain()
{
	PathPoints.Reset();
	PathIndex = 0;
	NextRepathTime = 0.0;
	bCharging = false;
	ChargeElapsed = 0.f;
	bAimJitterDrawn = false;
	SupportSpotIndex = -1;
	NextSupportSpotTime = 0.0;
	SupportAheadUntil = 0.0;
	BreachWall.Reset();
	CurrentIntent = EWB_SoccerAIIntent::Idle;
}

EWB_SoccerAIIntent AWB_SoccerAIController_Base::ChooseIntent_Implementation()
{
	// A forced role (gyms) comes before the coordinator and the legacy brain.
	EWB_SoccerAIIntent ForcedIntent = EWB_SoccerAIIntent::Idle;
	if (ChooseForcedIntent(GetForcedRole(), ForcedIntent))
	{
		return ForcedIntent;
	}

	if (PlayerStateComp && PlayerStateComp->IsHoldingAnything())
	{
		return EWB_SoccerAIIntent::Attack;
	}

	// A pass meant for this bot comes first, whatever its role.
	if (RefreshIncomingPass())
	{
		if (IsIntentAllowed(EWB_SoccerAIIntent::ReceivePass))
		{
			return EWB_SoccerAIIntent::ReceivePass;
		}
		// Debug filter: ReceivePass off, the receiver keeps the intent of its role.
		FilteredIntent = EWB_SoccerAIIntent::ReceivePass;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (Profile && Profile->bUseTeamRoles && CurrentRole != EWB_SoccerRole::None)
	{
		if (CurrentRole != EWB_SoccerRole::Attacker && PressTarget.IsValid())
		{
			if (IsIntentAllowed(EWB_SoccerAIIntent::Press))
			{
				return EWB_SoccerAIIntent::Press;
			}
			// Debug filter: Press off, the bot keeps the intent of its role.
			FilteredIntent = EWB_SoccerAIIntent::Press;
		}
		const bool bTeamHasBall = TeamSituation == EWB_TeamSituation::Attacking;
		switch (CurrentRole)
		{
		case EWB_SoccerRole::Defender:
			return bTeamHasBall ? EWB_SoccerAIIntent::SupportOpen : EWB_SoccerAIIntent::Defend;
		case EWB_SoccerRole::Goalkeeper:
			// A free ball inside its zone: the Goalkeeper claims it.
			if (!bTeamHasBall && Profile->bGoalkeeperClaimsBallInZone && CachedBall.IsValid() && !FindBallCarrier())
			{
				if (const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone())
				{
					if (Zone->IsLocationInside(CachedBall->GetActorLocation()))
					{
						if (IsIntentAllowed(EWB_SoccerAIIntent::SeekBall))
						{
							return EWB_SoccerAIIntent::SeekBall;
						}
						// Debug filter: SeekBall off, the Goalkeeper keeps guarding.
						FilteredIntent = EWB_SoccerAIIntent::SeekBall;
					}
				}
			}
			// The team holds the ball: the Goalkeeper becomes a defender only once the ball is in enemy territory.
			if (bTeamHasBall && IsBallInEnemyTerritory())
			{
				if (IsIntentAllowed(EWB_SoccerAIIntent::LastMan))
				{
					return EWB_SoccerAIIntent::LastMan;
				}
				// Debug filter: LastMan off, the Goalkeeper keeps guarding.
				FilteredIntent = EWB_SoccerAIIntent::LastMan;
			}
			return EWB_SoccerAIIntent::GuardGoal;
		default:
			break; // Attacker: same as the legacy brain below.
		}
	}

	if (const APawn* Carrier = FindBallCarrier())
	{
		return IsOpponent(Carrier) ? EWB_SoccerAIIntent::ChaseCarrier : EWB_SoccerAIIntent::Support;
	}
	return CachedBall.IsValid() ? EWB_SoccerAIIntent::SeekBall : EWB_SoccerAIIntent::Idle;
}

void AWB_SoccerAIController_Base::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ACharacter* Me = GetCharacter();
	if (!Me || !Commands || !PlayerStateComp || bBrainPaused)
	{
		return;
	}
	if (!AIProfile)
	{
		// No silent fallback: without its profile the bot does nothing.
		if (!bWarnedNoProfile)
		{
			bWarnedNoProfile = true;
			UE_LOG(LogTemp, Warning, TEXT("%s: no AIProfile (DA_SoccerAIProfile_Default expected). The bot stays idle."), *GetName());
		}
		Commands->StopMoving();
		return;
	}

	RefreshBall(Me);

	// A shot being charged: keep aiming and release on time.
	if (PlayerStateComp->IsInState(WBTags::Player_State_Kicking) || PlayerStateComp->IsInState(WBTags::Player_State_Throwing))
	{
		Commands->StopMoving();
		if (bCharging)
		{
			TickCharge(Me, DeltaSeconds);
		}
		return;
	}
	bCharging = false;

	// Every other state (Dashing, Stunned, Down, Disabled, Celebrating...) leaves no choice to the bot.
	// The defense stance still moves (RequestMove routes the move to the stance).
	if (!PlayerStateComp->IsInState(WBTags::Player_State_Free) && !PlayerStateComp->IsInState(WBTags::Player_State_Defending))
	{
		Commands->StopMoving();
		return;
	}

	// Debug filter: ChooseIntent skips the optional intents switched off (Press, Goalkeeper claim, LastMan); any other intent switched off becomes Idle.
	FilteredIntent = EWB_SoccerAIIntent::Idle;
	EWB_SoccerAIIntent NewIntent = ChooseIntent();
	if (!IsIntentAllowed(NewIntent))
	{
		FilteredIntent = NewIntent;
		NewIntent = EWB_SoccerAIIntent::Idle;
	}
	if (NewIntent != CurrentIntent)
	{
		const EWB_SoccerAIIntent OldIntent = CurrentIntent;
		CurrentIntent = NewIntent;
		PathPoints.Reset();
		bAimJitterDrawn = false;
		OnIntentChanged(OldIntent, NewIntent);
	}

	UpdateGoalkeeperStance(Me);

	switch (CurrentIntent)
	{
	case EWB_SoccerAIIntent::SeekBall:     TickSeekBall(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::ChaseCarrier: TickChaseCarrier(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::Support:      TickSupport(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::Attack:       TickAttack(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::Press:        TickPress(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::Defend:       TickDefend(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::GuardGoal:    TickGuardGoal(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::LastMan:      TickLastMan(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::SupportOpen:  TickSupportOpen(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::ReceivePass:  TickReceivePass(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::KeepAway:     TickKeepAway(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::ClearBall:    TickClearBall(Me, DeltaSeconds); break;
	case EWB_SoccerAIIntent::Idle:         TickIdle(Me, DeltaSeconds); break;
	default:                               Commands->StopMoving(); break;
	}

	DrawDebug(Me);
}

void AWB_SoccerAIController_Base::RefreshBall(const ACharacter* Me)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (CachedBall.IsValid() && Now < NextBallSearchTime)
	{
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	NextBallSearchTime = Now + Profile->BallSearchInterval;

	UClass* BallClass = Profile->BallClass.LoadSynchronous();
	if (!BallClass)
	{
		CachedBall.Reset();
		return;
	}

	// The balls register themselves in the possession subsystem (BeginPlay of BP_PO_Master_Ball_01): no search through the level.
	TArray<AActor*> Balls;
	UWB_BallPossessionSubsystem::GetRegisteredBalls(this, Balls);
	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (AActor* Ball : Balls)
	{
		if (!Ball->IsA(BallClass))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Ball->GetActorLocation(), Me->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Ball;
		}
	}
	CachedBall = Best;
}

void AWB_SoccerAIController_Base::TickIdle(ACharacter* Me, float DeltaSeconds)
{
	Commands->StopMoving();
	// In the defense stance the facing is locked by UWB_PlayerDiveComponent.
	const AActor* Ball = CachedBall.Get();
	if (Ball && !(DiveComp && DiveComp->IsDefending()))
	{
		const FVector ToBall = (Ball->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
		if (!ToBall.IsNearlyZero())
		{
			TurnToward(Me, ToBall, DeltaSeconds);
		}
	}
}

void AWB_SoccerAIController_Base::TickSeekBall(ACharacter* Me, float DeltaSeconds)
{
	const AActor* Ball = CachedBall.Get();
	if (!Ball)
	{
		Commands->StopMoving();
		return;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector ToBall = Ball->GetActorLocation() - Me->GetActorLocation();
	if (!IsActionAllowed(Profile, Profile->bAllowBallPickup))
	{
		// Debug filter: pickup off, stop GrabRange away from the ball (on the bot side) and face it.
		const FVector Away = ToBall.IsNearlyZero() ? -Me->GetActorForwardVector() : -ToBall.GetSafeNormal2D();
		HoldSpot(Me, Ball->GetActorLocation() + Away * Profile->GrabRange, Ball->GetActorLocation(), DeltaSeconds);
		return;
	}
	if (ToBall.Size2D() <= Profile->GrabRange)
	{
		// Close: walk straight into the ball, the catch zone in front of the player takes it automatically.
		Commands->MoveInWorldDirection(ToBall);
		return;
	}
	MoveTowardLocation(Me, Ball->GetActorLocation());
}

bool AWB_SoccerAIController_Base::RefreshIncomingPass()
{
	IncomingPassBall.Reset();
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile || !Profile->bReceivePasses)
	{
		return false;
	}
	float PassAge = 0.f;
	AActor* Ball = UWB_BallPossessionSubsystem::FindBallPassedTo(this, GetPawn(), PassAge);
	if (!Ball || PassAge > Profile->ReceivePassMaxTime)
	{
		return false;
	}
	IncomingPassBall = Ball;
	return true;
}

FVector AWB_SoccerAIController_Base::GetPassInterceptPoint(const ACharacter* Me, const FVector& BallLocation, const FVector& BallVelocity) const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector Flat(BallVelocity.X, BallVelocity.Y, 0.f);
	const float Speed = Flat.Size();
	// Under 1 cm/s the ball is still: go to it. Technical value.
	if (Speed < 1.f)
	{
		return BallLocation;
	}
	const FVector Direction = Flat / Speed;
	// Same slow-down as the pass computation (one place for the setting): DA_Pass_Default on the bot's pawn.
	const UWB_PassProfile* PassProfile = Commands ? Commands->PassProfile.Get() : nullptr;
	const float Deceleration = PassProfile ? PassProfile->PassRollingDeceleration : 0.f;
	const float Horizon = Deceleration > 0.f ? FMath::Min(Speed / Deceleration, Profile->ReceivePassMaxTime) : Profile->ReceivePassMaxTime;

	const UCharacterMovementComponent* Movement = Me->GetCharacterMovement();
	const float MySpeed = Movement ? Movement->GetMaxSpeed() : 0.f;
	const FVector MyLocation = Me->GetActorLocation();

	auto PointAt = [&](float Time)
	{
		const float Travel = Speed * Time - 0.5f * Deceleration * Time * Time;
		return BallLocation + Direction * Travel;
	};

	// Path sampled every 0.05 s. Technical value.
	constexpr float Step = 0.05f;
	if (MySpeed > 1.f)
	{
		for (float Time = 0.f; Time <= Horizon; Time += Step)
		{
			const FVector Point = PointAt(Time);
			if (FVector::Dist2D(MyLocation, Point) / MySpeed + Profile->ReceivePassMargin <= Time)
			{
				return Point;
			}
		}
	}
	return PointAt(Horizon);
}

void AWB_SoccerAIController_Base::TickReceivePass(ACharacter* Me, float DeltaSeconds)
{
	const AActor* Ball = IncomingPassBall.Get();
	if (!Ball)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const UPrimitiveComponent* Body = UWB_BallPossessionSubsystem::FindBallBody(Ball);
	const FVector BallLocation = Body ? Body->Bounds.Origin : Ball->GetActorLocation();
	const FVector BallVelocity = Body ? Body->GetComponentVelocity() : Ball->GetVelocity();

	// Close: walk straight into the ball, the catch is automatic.
	const FVector ToBall = BallLocation - Me->GetActorLocation();
	if (ToBall.Size2D() <= Profile->GrabRange)
	{
		PassInterceptPoint = BallLocation;
		Commands->MoveInWorldDirection(ToBall);
		return;
	}
	PassInterceptPoint = GetPassInterceptPoint(Me, BallLocation, BallVelocity);
	HoldSpot(Me, PassInterceptPoint, BallLocation, DeltaSeconds);
}

void AWB_SoccerAIController_Base::TickChaseCarrier(ACharacter* Me, float DeltaSeconds)
{
	APawn* Carrier = FindBallCarrier();
	if (!Carrier)
	{
		Commands->StopMoving();
		return;
	}
	ChaseAndDash(Me, Carrier);
}

void AWB_SoccerAIController_Base::ChaseAndDash(ACharacter* Me, APawn* Target)
{
	if (TryDashOn(Me, Target))
	{
		return;
	}
	// Predicted spot of the carrier, computed by the EvaluateDash of TryDashOn.
	const FVector AimPoint = LastDashAimPoint;
	// Too close for the minimum run-up: step back to make room for the Dash.
	if (DashBlockReason == TEXT("Too close"))
	{
		Commands->MoveInWorldDirection(Me->GetActorLocation() - AimPoint);
		return;
	}
	// Not favorable yet: keep chasing, toward where the carrier is going (the path finds a way around a blocked line).
	MoveTowardLocation(Me, AimPoint);
}

bool AWB_SoccerAIController_Base::TryDashOn(ACharacter* Me, APawn* Target)
{
	FVector AimPoint;
	if (!EvaluateDash(Target, AimPoint))
	{
		return false;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!IsActionAllowed(Profile, Profile->bAllowDash))
	{
		// Debug filter: Dash off, the bot keeps following the target (ChaseAndDash moves toward the predicted spot).
		static const FName FilterOff(TEXT("Dash off (filter)"));
		DashBlockReason = FilterOff;
		return false;
	}
	// Dash along the actor forward: face the predicted spot with a random error, so some Dashes miss.
	NextDashTime = GetWorld()->GetTimeSeconds() + GetDashCooldown();
	const float Jitter = FMath::FRandRange(-Profile->DashAngleJitter, Profile->DashAngleJitter);
	Commands->StopMoving();
	const FVector AimLocation = Profile->bDashAimAtTargetLocation ? Target->GetActorLocation() : AimPoint;
	Commands->SetFacingDirection((AimLocation - Me->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(Jitter, FVector::UpVector));
	Commands->DashWithCharge(Profile->BotDashCharge);
	return true;
}

bool AWB_SoccerAIController_Base::EvaluateDash(APawn* Target, FVector& AimPoint)
{
	static const FName Ready(TEXT("Ready"));
	static const FName NoTarget(TEXT("No target"));
	static const FName Cooldown(TEXT("Cooldown"));
	static const FName CannotDash(TEXT("Cannot Dash (state)"));
	static const FName Exhausted(TEXT("Exhausted"));
	static const FName LowEnergy(TEXT("Low energy"));
	static const FName Protected(TEXT("Target protected"));
	static const FName OutOfReach(TEXT("Out of reach"));
	static const FName TooClose(TEXT("Too close"));

	const ACharacter* Me = GetCharacter();
	AimPoint = Target ? Target->GetActorLocation() : FVector::ZeroVector;
	LastDashAimPoint = AimPoint;
	if (!Me || !Target || !Commands || !PlayerStateComp || !AIProfile || !Commands->GetDashProfile())
	{
		DashBlockReason = NoTarget;
		return false;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector MyLocation = Me->GetActorLocation();

	// Lead the target first: the chase uses the predicted spot even when no Dash is possible.
	// Reach and speed of the bot's Dash on a player, at its charge (BotDashCharge).
	const float DashDistance = Commands->GetDashDistanceForCharge(Profile->BotDashCharge);
	const float DashSpeed = DashDistance / FMath::Max(Commands->GetDashDurationForCharge(Profile->BotDashCharge), UE_KINDA_SMALL_NUMBER);
	const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	const float Radii = Me->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ (TargetCharacter ? TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f);
	const FVector TargetVelocity = Target->GetVelocity() * FVector(1.f, 1.f, 0.f);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		// Travel time until the capsules touch, refined once with the predicted spot.
		const float Travel = FMath::Max(0.f, FVector::Dist2D(MyLocation, AimPoint) - Radii) / DashSpeed;
		AimPoint = Target->GetActorLocation() + TargetVelocity * Travel * Profile->DashLeadFactor;
	}
	LastDashAimPoint = AimPoint;

	// Own side: cooldown, state (Free, empty hands), energy.
	if (GetWorld()->GetTimeSeconds() < NextDashTime)
	{
		DashBlockReason = Cooldown;
		return false;
	}
	if (!PlayerStateComp->CanEnterState(WBTags::Player_State_Dashing))
	{
		DashBlockReason = CannotDash;
		return false;
	}
	if (Commands->IsExhausted())
	{
		DashBlockReason = Exhausted;
		return false;
	}
	if (Commands->GetEnergyPercent() < Commands->GetDashEnergyCost() + Profile->DashEnergyReserve)
	{
		DashBlockReason = LowEnergy;
		return false;
	}

	// Target side: Down, post-Down grace, hit cooldown, friendly fire.
	const UWB_PlayerHitComponent* TargetHit = Target->FindComponentByClass<UWB_PlayerHitComponent>();
	if (TargetHit && !TargetHit->CanBeDashedBy(const_cast<ACharacter*>(Me)))
	{
		DashBlockReason = Protected;
		return false;
	}

	// Reach: the capsules must touch before the Dash ends, after the minimum run-up.
	const float Distance = FVector::Dist2D(MyLocation, AimPoint);
	if (Distance > (DashDistance + Radii) * Profile->DashReachRatio)
	{
		DashBlockReason = OutOfReach;
		return false;
	}
	if (Distance < Commands->GetDashMinRunUp() + Radii + Profile->DashRunUpMargin)
	{
		DashBlockReason = TooClose;
		return false;
	}

	if (IsDashLineBlocked(Me, Target, AimPoint, FMath::Min(Distance, DashDistance)))
	{
		return false;
	}
	DashBlockReason = Ready;
	return true;
}

bool AWB_SoccerAIController_Base::IsDashLineBlocked(const ACharacter* Me, const APawn* Target, const FVector& AimPoint, float Length)
{
	static const FName Blocked(TEXT("Line blocked"));
	static const FName Teammate(TEXT("Teammate in line"));

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile->bDashNeedsClearLine && !Profile->bDashAvoidTeammates)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = Me->GetCapsuleComponent();
	const FVector Start = Me->GetActorLocation();
	const FVector End = Start + (AimPoint - Start).GetSafeNormal2D() * Length;

	// Same shape and collision as the Dash (capsule sweep of SetActorLocation). Pushed things are ignored and the sweep goes on.
	FComponentQueryParams Params(SCENE_QUERY_STAT(WBSoccerBotDashLine), Me);
	Params.AddIgnoredActor(Target);
	for (int32 Step = 0; Step < 4; ++Step)
	{
		TArray<FHitResult> Hits;
		World->ComponentSweepMulti(Hits, Capsule, Start, End, Capsule->GetComponentQuat(), Params);
		const FHitResult* Block = Hits.FindByPredicate([](const FHitResult& Hit) { return Hit.bBlockingHit && !Hit.bStartPenetrating; });
		if (!Block)
		{
			return false;
		}
		AActor* HitActor = Block->GetActor();

		// Floor or slope: not in the way.
		if (Block->ImpactNormal.Z > 0.7f)
		{
			Params.AddIgnoredActor(HitActor);
			continue;
		}

		// Another player: an opponent is a bonus, a teammate the Dash would knock Down is not.
		if (const UWB_PlayerHitComponent* OtherHit = HitActor ? HitActor->FindComponentByClass<UWB_PlayerHitComponent>() : nullptr)
		{
			if (Profile->bDashAvoidTeammates && !IsOpponent(Cast<APawn>(HitActor)) && OtherHit->CanBeDashedBy(const_cast<ACharacter*>(Me)))
			{
				DashBlockReason = Teammate;
				return true;
			}
			Params.AddIgnoredActor(HitActor);
			continue;
		}

		// Ball and physics objects are pushed by the Dash.
		const UPrimitiveComponent* HitComponent = Block->GetComponent();
		if (HitComponent && HitComponent->IsSimulatingPhysics())
		{
			Params.AddIgnoredActor(HitActor);
			continue;
		}

		if (Profile->bDashNeedsClearLine)
		{
			DashBlockReason = Blocked;
			return true;
		}
		Params.AddIgnoredActor(HitActor);
	}
	return false;
}

void AWB_SoccerAIController_Base::TickSupport(ACharacter* Me, float DeltaSeconds)
{
	const APawn* Anchor = FindBallCarrier();
	if (!Anchor)
	{
		Commands->StopMoving();
		return;
	}
	MoveToSupportSpot(Me, Anchor->GetActorLocation());
}

void AWB_SoccerAIController_Base::MoveToSupportSpot(ACharacter* Me, const FVector& Anchor)
{
	const UWB_TeamGoalComponent* Goal = GetGoalToAttack();
	if (!Goal)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector Forward = (Goal->GetAimLocation() - Anchor).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	// Stay on the side the bot is already on.
	const float SideSign = FVector::DotProduct(Me->GetActorLocation() - Anchor, Side) >= 0.f ? 1.f : -1.f;
	const FVector Spot = Anchor + Forward * Profile->SupportLeadDistance + Side * SideSign * Profile->SupportLateralOffset;
	MoveTowardLocation(Me, Spot);
}

bool AWB_SoccerAIController_Base::IsBallInEnemyTerritory() const
{
	const AActor* Ball = CachedBall.Get();
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	const UWB_TeamGoalComponent* OtherGoal = GetGoalToAttack();
	if (!Ball || !OwnGoal || !OtherGoal)
	{
		return false;
	}
	const FVector Axis = (OtherGoal->GetAimLocation() - OwnGoal->GetAimLocation()) * FVector(1.f, 1.f, 0.f);
	const double LengthSq = Axis.SizeSquared();
	if (LengthSq < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// Share of the way along the goal - goal axis, 0 at the own goal, 1 at the opponent goal.
	const double Share = FVector::DotProduct(Ball->GetActorLocation() - OwnGoal->GetAimLocation(), Axis) / LengthSq;
	return Share > GetActiveProfile()->GoalkeeperAdvanceFieldShare;
}

bool AWB_SoccerAIController_Base::GetThreatLocation(FVector& OutLocation) const
{
	if (const APawn* Carrier = FindBallCarrier())
	{
		if (IsOpponent(Carrier))
		{
			OutLocation = Carrier->GetActorLocation();
			return true;
		}
	}
	if (const AActor* Ball = CachedBall.Get())
	{
		OutLocation = Ball->GetActorLocation();
		return true;
	}
	return false;
}

void AWB_SoccerAIController_Base::HoldSpot(ACharacter* Me, const FVector& Spot, const FVector& LookAt, float DeltaSeconds)
{
	// In the defense stance the facing is locked by UWB_PlayerDiveComponent.
	if (MoveTowardLocation(Me, Spot) && !(DiveComp && DiveComp->IsDefending()))
	{
		const FVector ToLook = (LookAt - Me->GetActorLocation()).GetSafeNormal2D();
		if (!ToLook.IsNearlyZero())
		{
			TurnToward(Me, ToLook, DeltaSeconds);
		}
	}
}

void AWB_SoccerAIController_Base::TickPress(ACharacter* Me, float DeltaSeconds)
{
	APawn* Target = PressTarget.Get();
	if (!Target)
	{
		Commands->StopMoving();
		return;
	}
	// The target holds the ball: same chase and Dash as on a carrier.
	if (IsHoldingBall(Target->FindComponentByClass<UWB_PlayerStateComponent>()))
	{
		ChaseAndDash(Me, Target);
		return;
	}
	// Otherwise mark it: stand between it and the ball (or its goal side when there is no ball).
	const FVector TargetLocation = Target->GetActorLocation();
	FVector Side = FVector::ZeroVector;
	if (const AActor* Ball = CachedBall.Get())
	{
		Side = (Ball->GetActorLocation() - TargetLocation).GetSafeNormal2D();
	}
	else if (const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend())
	{
		Side = (OwnGoal->GetAimLocation() - TargetLocation).GetSafeNormal2D();
	}
	HoldSpot(Me, TargetLocation + Side * GetActiveProfile()->PressMarkDistance, TargetLocation, DeltaSeconds);
}

void AWB_SoccerAIController_Base::TickDefend(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	FVector Threat;
	if (!OwnGoal || !GetThreatLocation(Threat))
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector GoalLocation = OwnGoal->GetAimLocation();

	const FVector ToThreat = (Threat - GoalLocation) * FVector(1.f, 1.f, 0.f);
	const FVector Direction = ToThreat.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToThreat.GetSafeNormal();

	// Defensive zone: from the net perimeter, half the distance between the two goals (times DefendRadiusScale).
	const float Perimeter = GetPerimeterDistance(Direction);
	float MaxDistance = TNumericLimits<float>::Max();
	if (const UWB_TeamGoalComponent* OtherGoal = GetGoalToAttack())
	{
		MaxDistance = Perimeter + FVector::Dist2D(GoalLocation, OtherGoal->GetAimLocation()) * 0.5f * Profile->DefendRadiusScale;
	}

	// On the line threat - goal, DefendGapToBall short of the threat, between the perimeter and the end of the zone.
	const float Distance = FMath::Clamp(static_cast<float>(ToThreat.Size()) - Profile->DefendGapToBall, Perimeter, FMath::Max(Perimeter, MaxDistance));
	HoldSpot(Me, KeepTeammateDistance(Me, GoalLocation + Direction * Distance), Threat, DeltaSeconds);
}

FVector AWB_SoccerAIController_Base::KeepTeammateDistance(const ACharacter* Me, const FVector& Spot) const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile->bDefenderKeepsTeammateDistance || CurrentRole != EWB_SoccerRole::Defender)
	{
		return Spot;
	}
	const UWB_TeamCoordinatorComponent* Coordinator = UWB_TeamCoordinatorComponent::GetTeamCoordinator(this, GetTeamIndex());
	if (!Coordinator)
	{
		return Spot;
	}

	// Teammate to follow: the Attacker, else the one who presses.
	const APawn* Anchor = nullptr;
	for (const FWB_TeamRoleAssignment& Entry : Coordinator->GetAssignments())
	{
		if (!Entry.Member || Entry.Member.Get() == Me)
		{
			continue;
		}
		if (Entry.Role == EWB_SoccerRole::Attacker)
		{
			Anchor = Entry.Member.Get();
			break;
		}
		if (Entry.bPressing && !Anchor)
		{
			Anchor = Entry.Member.Get();
		}
	}
	if (!Anchor)
	{
		return Spot;
	}

	const FVector AnchorLocation = Anchor->GetActorLocation();
	FVector Offset = (Spot - AnchorLocation) * FVector(1.f, 1.f, 0.f);
	if (Offset.IsNearlyZero())
	{
		Offset = (Me->GetActorLocation() - AnchorLocation) * FVector(1.f, 1.f, 0.f);
	}
	if (Offset.IsNearlyZero())
	{
		return Spot;
	}

	const float MinDistance = FMath::Max(Profile->DefenderTeammateDistance - Profile->DefenderTeammateTolerance, 0.f);
	const float MaxDistance = Profile->DefenderTeammateDistance + Profile->DefenderTeammateTolerance;
	const float Distance = static_cast<float>(Offset.Size());
	if (Distance >= MinDistance && Distance <= MaxDistance)
	{
		return Spot;
	}
	const FVector Adjusted = AnchorLocation + Offset.GetSafeNormal() * FMath::Clamp(Distance, MinDistance, MaxDistance);
	return FVector(Adjusted.X, Adjusted.Y, Spot.Z);
}

void AWB_SoccerAIController_Base::TickGuardGoal(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	FVector Threat;
	const bool bHasThreat = GetThreatLocation(Threat);
	const FVector ToThreat = bHasThreat ? (Threat - GoalLocation).GetSafeNormal2D() : FVector::ZeroVector;
	const FVector Direction = ToThreat.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToThreat;

	// GoalkeeperDistance out of the net perimeter; inside the zone once the threat comes within GoalkeeperRetreatDistance of the perimeter.
	const float Perimeter = GetPerimeterDistance(Direction);
	float Distance = Perimeter + Profile->GoalkeeperDistance;
	if (bHasThreat && Perimeter > 0.f && FVector::Dist2D(Threat, GoalLocation) - Perimeter <= Profile->GoalkeeperRetreatDistance)
	{
		Distance = FMath::Max(0.f, Perimeter - Profile->GoalkeeperInsideDepth);
	}
	HoldSpot(Me, GoalLocation + Direction * Distance, bHasThreat ? Threat : GoalLocation + Direction * 1000.f, DeltaSeconds);
}

void AWB_SoccerAIController_Base::TickLastMan(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	const AActor* Ball = CachedBall.Get();
	if (!OwnGoal || !Ball)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector ToBall = (Ball->GetActorLocation() - GoalLocation) * FVector(1.f, 1.f, 0.f);
	float Distance = static_cast<float>(ToBall.Size()) * Profile->LastManBallShare;

	// Stay the member nearest to the own goal: behind the teammate closest to it.
	if (const UWB_TeamCoordinatorComponent* Coordinator = UWB_TeamCoordinatorComponent::GetTeamCoordinator(this, GetTeamIndex()))
	{
		for (const FWB_TeamRoleAssignment& Entry : Coordinator->GetAssignments())
		{
			if (Entry.Member && Entry.Member.Get() != Me)
			{
				Distance = FMath::Min(Distance, static_cast<float>(FVector::Dist2D(Entry.Member->GetActorLocation(), GoalLocation)) - Profile->LastManBehindMargin);
			}
		}
	}
	const FVector Direction = ToBall.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToBall.GetSafeNormal();
	Distance = FMath::Max(Distance, GetPerimeterDistance(Direction) + Profile->GoalkeeperDistance);
	HoldSpot(Me, GoalLocation + Direction * Distance, Ball->GetActorLocation(), DeltaSeconds);
}

void AWB_SoccerAIController_Base::UpdateGoalkeeperStance(ACharacter* Me)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	// The forced Goalkeeper (gyms) takes the stance even without the team roles.
	const bool bRolesOn = Profile->bUseTeamRoles || GetForcedRole().MatchesTagExact(WBTags::AI_ForcedRole_Goalkeeper);
	if (!DiveComp || !bRolesOn || !Profile->bGoalkeeperAutoDefense)
	{
		return;
	}
	const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone();
	const bool bDefending = DiveComp->IsDefending();
	const bool bGuarding = CurrentRole == EWB_SoccerRole::Goalkeeper
		&& (CurrentIntent == EWB_SoccerAIIntent::GuardGoal || CurrentIntent == EWB_SoccerAIIntent::SeekBall);
	// Hysteresis: once in the stance, it is kept until GoalkeeperZoneExitMargin out of the zone.
	const bool bInZone = Zone && Zone->IsLocationInside(Me->GetActorLocation(), bDefending ? Profile->GoalkeeperZoneExitMargin : 0.f);
	const bool bWant = bGuarding && bInZone;

	if (bWant && !bDefending && PlayerStateComp->IsInState(WBTags::Player_State_Free) && !PlayerStateComp->IsHoldingAnything())
	{
		DiveComp->TryStartDefense();
	}
	else if (!bWant && bDefending)
	{
		DiveComp->StopDefense();
	}

	// Goalkeeper in its zone: always faces the ball (the stance would otherwise keep its first facing).
	const AActor* Ball = CachedBall.Get();
	if (Ball && CurrentRole == EWB_SoccerRole::Goalkeeper && DiveComp->IsDefending() && bInZone)
	{
		DiveComp->SetDefenseFacing(Ball->GetActorLocation() - Me->GetActorLocation());
	}
}

FVector AWB_SoccerAIController_Base::GetSupportSpot(const FVector& CarrierLocation, int32 Index) const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const float Angle = 360.f * Index / FMath::Max(1, Profile->SupportOpenSampleCount);
	const FVector Spot = CarrierLocation + FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * Profile->SupportOpenDistance;
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (NavSys && NavSys->ProjectPointToNavigation(Spot, Projected, Profile->NavProjectExtent))
	{
		return Projected.Location;
	}
	return Spot;
}

void AWB_SoccerAIController_Base::GatherOpponentLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (*It != GetPawn() && It->FindComponentByClass<UWB_PlayerStateComponent>() && IsOpponent(*It))
		{
			OutLocations.Add(It->GetActorLocation());
		}
	}
}

float AWB_SoccerAIController_Base::ScoreSupportSpot(const FVector& CarrierLocation, const FVector& Spot, const FVector& Forward, const TArray<FVector>& OpponentLocations) const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	float NearestOpponent = Profile->SupportOpenDistance * 2.f;
	float NearestToLane = Profile->SupportOpenDistance;
	for (const FVector& Location : OpponentLocations)
	{
		NearestOpponent = FMath::Min(NearestOpponent, static_cast<float>(FVector::Dist2D(Location, Spot)));
		const FVector OnLane = FMath::ClosestPointOnSegment(Location, CarrierLocation, Spot);
		NearestToLane = FMath::Min(NearestToLane, static_cast<float>(FVector::Dist2D(Location, OnLane)));
	}
	const float Toward = FVector::DotProduct((Spot - CarrierLocation).GetSafeNormal2D(), Forward);
	return NearestOpponent + Profile->SupportOpenLaneWeight * NearestToLane + Profile->SupportOpenForwardBias * Toward;
}

void AWB_SoccerAIController_Base::TickSupportOpen(ACharacter* Me, float DeltaSeconds)
{
	const APawn* Carrier = FindBallCarrier();
	const UWB_TeamGoalComponent* Goal = GetGoalToAttack();
	if (!Carrier || !Goal)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector CarrierLocation = Carrier->GetActorLocation();
	const FVector Forward = (Goal->GetAimLocation() - CarrierLocation).GetSafeNormal2D();

	// Sometimes, run ahead of the ball, when that spot is in shooting range.
	if (Now >= NextSupportAheadRollTime)
	{
		NextSupportAheadRollTime = Now + Profile->SupportAheadRollInterval;
		const FVector AheadSpot = CarrierLocation + Forward * Profile->SupportLeadDistance;
		if (FVector::Dist2D(AheadSpot, Goal->GetAimLocation()) <= Profile->ShootMaxDistance && FMath::FRand() < Profile->SupportAheadChance)
		{
			SupportAheadUntil = Now + Profile->SupportAheadDuration;
		}
	}
	if (Now < SupportAheadUntil)
	{
		MoveToSupportSpot(Me, CarrierLocation);
		return;
	}

	// Open spot around the carrier: far from the opponents and from the pass lane, kept unless a spot is clearly better.
	if (SupportSpotIndex < 0 || Now >= NextSupportSpotTime)
	{
		NextSupportSpotTime = Now + Profile->SupportOpenInterval;
		TArray<FVector> OpponentLocations;
		GatherOpponentLocations(OpponentLocations);
		int32 BestIndex = -1;
		float BestScore = -TNumericLimits<float>::Max();
		float CurrentScore = -TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Profile->SupportOpenSampleCount; ++Index)
		{
			const float Score = ScoreSupportSpot(CarrierLocation, GetSupportSpot(CarrierLocation, Index), Forward, OpponentLocations);
			if (Index == SupportSpotIndex)
			{
				CurrentScore = Score;
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				BestIndex = Index;
			}
		}
		if (SupportSpotIndex < 0 || BestScore > CurrentScore + Profile->SupportOpenSwitchGain)
		{
			SupportSpotIndex = BestIndex;
		}
	}
	HoldSpot(Me, GetSupportSpot(CarrierLocation, SupportSpotIndex), CarrierLocation, DeltaSeconds);
}

FVector AWB_SoccerAIController_Base::GetShotDirection(const ACharacter* Me, const UWB_TeamGoalComponent* Goal)
{
	if (!bAimJitterDrawn)
	{
		const float Jitter = GetActiveProfile()->ShootAimJitter;
		AimJitterYaw = FMath::FRandRange(-Jitter, Jitter);
		bAimJitterDrawn = true;
	}
	return (Goal->GetAimLocation() - Me->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(AimJitterYaw, FVector::UpVector);
}

void AWB_SoccerAIController_Base::TickAttack(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* Goal = GetGoalToAttack();
	if (!Goal)
	{
		Commands->StopMoving();
		return;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector Aim = Goal->GetAimLocation();
	const FVector FieldDirection = Goal->GetFieldDirection();
	const FVector FromGoal = (Me->GetActorLocation() - Aim) * FVector(1.f, 1.f, 0.f);
	const float Distance = FromGoal.Size();
	const FVector FromGoalDir = FromGoal.GetSafeNormal();
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FromGoalDir, FieldDirection), -1.f, 1.f)));

	const bool bInZone = Distance >= Profile->ShootMinDistance && Distance <= Profile->ShootMaxDistance && Angle <= Profile->ShootConeHalfAngle;
	if (!bInZone)
	{
		// Run to the spot of the zone closest to the current side.
		const FVector SpotDir = ClampDirectionToCone(FromGoalDir.IsNearlyZero() ? FieldDirection : FromGoalDir, FieldDirection, Profile->ShootConeHalfAngle * Profile->ShootSpotConeScale);
		MoveTowardLocation(Me, Aim + SpotDir * Profile->ShootPreferredDistance);
		return;
	}

	Commands->StopMoving();
	const float Remaining = TurnToward(Me, GetShotDirection(Me, Goal), DeltaSeconds);
	// Debug filter: shoot off, the bot aims at the goal and waits.
	if (Remaining <= Profile->ShootAimTolerance && IsActionAllowed(Profile, Profile->bAllowShoot))
	{
		const float Alpha = Profile->ShootMaxDistance > Profile->ShootMinDistance
			? FMath::Clamp((Distance - Profile->ShootMinDistance) / (Profile->ShootMaxDistance - Profile->ShootMinDistance), 0.f, 1.f)
			: 1.f;
		ChargeTarget = FMath::Lerp(Profile->ChargeTimeAtMinDistance, Profile->ChargeTimeAtMaxDistance, Alpha);
		ChargeElapsed = 0.f;
		bCharging = true;
		Commands->KickPress();
	}
}

void AWB_SoccerAIController_Base::TickCharge(ACharacter* Me, float DeltaSeconds)
{
	if (const UWB_TeamGoalComponent* Goal = GetGoalToAttack())
	{
		Commands->SetFacingDirection(GetShotDirection(Me, Goal));
	}

	ChargeElapsed += DeltaSeconds;
	if (ChargeElapsed >= ChargeTarget)
	{
		Commands->KickRelease();
		bCharging = false;
		bAimJitterDrawn = false;
	}
}

float AWB_SoccerAIController_Base::TurnToward(ACharacter* Me, const FVector& Direction, float DeltaSeconds)
{
	const float TargetYaw = Direction.Rotation().Yaw;
	const float NewYaw = FMath::FixedTurn(Me->GetActorRotation().Yaw, TargetYaw, GetActiveProfile()->AimTurnSpeed * DeltaSeconds);
	Commands->SetFacingDirection(FRotator(0.f, NewYaw, 0.f).Vector());
	return FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw));
}

void AWB_SoccerAIController_Base::RebuildPath(ACharacter* Me, const FVector& Target)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector Start = Me->GetActorLocation();
	PathTarget = Target;
	PathPoints.Reset();
	PathIndex = 1;
	NextRepathTime = GetWorld()->GetTimeSeconds() + Profile->RepathInterval;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = NavSys ? NavSys->GetNavDataForProps(Me->GetNavAgentPropertiesRef()) : nullptr;
	if (NavData)
	{
		FVector End = Target;
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Target, Projected, Profile->NavProjectExtent))
		{
			End = Projected.Location;
		}

		FPathFindingQuery Query(this, *NavData, Start, End);
		Query.SetAllowPartialPaths(true);
		const FPathFindingResult Result = NavSys->FindPathSync(Query);
		bool bPartial = true;
		if (Result.IsSuccessful() && Result.Path.IsValid())
		{
			bPartial = Result.Path->IsPartial();
			for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
			{
				PathPoints.Add(Point.Location);
			}
		}
		float PathLength = 0.f;
		for (int32 i = 1; i < PathPoints.Num(); ++i)
		{
			PathLength += FVector::Dist2D(PathPoints[i - 1], PathPoints[i]);
		}
		EvaluateBreach(Me, Target, PathLength, bPartial || PathPoints.Num() < 2);
	}
	else
	{
		BreachWall.Reset();
	}

	// No NavMesh or no path: straight line.
	if (PathPoints.Num() < 2)
	{
		PathPoints = { Start, Target };
	}
}

bool AWB_SoccerAIController_Base::IsBreakable(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}
	if (const UWB_HingedDoorComponent* Door = Actor->FindComponentByClass<UWB_HingedDoorComponent>())
	{
		return Door->IsLocked();
	}
	return Actor->FindComponentByClass<UGeometryCollectionComponent>() != nullptr;
}

bool AWB_SoccerAIController_Base::CanDashNow() const
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	return Profile && Commands && PlayerStateComp && GetWorld()->GetTimeSeconds() >= NextBreachDashTime
		&& PlayerStateComp->CanEnterState(WBTags::Player_State_Dashing) && !Commands->IsExhausted()
		&& Commands->GetEnergyPercent() >= Commands->GetDashEnergyCost() + Profile->DashEnergyReserve;
}

void AWB_SoccerAIController_Base::EvaluateBreach(ACharacter* Me, const FVector& Target, float PathLength, bool bPartial)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	// The breach is a Dash: the debug filter switch bAllowDash rules it out too.
	if (!Profile->bAllowBreach || !IsActionAllowed(Profile, Profile->bAllowDash) || !Commands || !PlayerStateComp || PlayerStateComp->IsHoldingAnything())
	{
		BreachWall.Reset();
		return;
	}
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	for (auto It = IgnoredBreachWalls.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || Now >= It->Value)
		{
			It.RemoveCurrent();
		}
	}

	// Walls on the straight line, at the bot height: players, the ball, physics objects (furniture, unlocked doors) and floors are skipped.
	const FVector Start = Me->GetActorLocation() + FVector(0.f, 0.f, Profile->BreachTraceHeight);
	const FVector End(Target.X, Target.Y, Start.Z);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBSoccerBotBreach), false, Me);
	int32 WallCount = 0;
	AActor* FirstWall = nullptr;
	FVector FirstPoint = FVector::ZeroVector;
	for (int32 Step = 0; Step < Profile->BreachMaxWalls + 8; ++Step)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params))
		{
			break;
		}
		AActor* HitActor = Hit.GetActor();
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		Params.AddIgnoredActor(HitActor);
		if (Cast<APawn>(HitActor) || (HitComponent && HitComponent->IsSimulatingPhysics()) || Hit.ImpactNormal.Z > 0.7f)
		{
			continue;
		}
		// A wall that cannot break (or one the bot gave up on) rules the straight line out.
		if (!IsBreakable(HitActor) || IgnoredBreachWalls.Contains(HitActor))
		{
			BreachWall.Reset();
			return;
		}
		if (!FirstWall)
		{
			FirstWall = HitActor;
			FirstPoint = Hit.ImpactPoint;
		}
		if (++WallCount > Profile->BreachMaxWalls)
		{
			BreachWall.Reset();
			return;
		}
	}
	if (!FirstWall)
	{
		BreachWall.Reset();
		return;
	}

	// Fastest way: the detour through the doors, or the straight line plus the time to break each wall.
	const float Speed = FMath::Max(1.f, Me->GetCharacterMovement() ? Me->GetCharacterMovement()->GetMaxSpeed() : 1.f);
	const float StraightTime = FVector::Dist2D(Me->GetActorLocation(), Target) / Speed + WallCount * Profile->BreachWallCostSeconds;
	const float DetourTime = bPartial ? TNumericLimits<float>::Max() : PathLength / Speed;
	if (StraightTime < DetourTime)
	{
		if (BreachWall.Get() != FirstWall)
		{
			BreachStartTime = Now;
		}
		BreachWall = FirstWall;
		BreachPoint = FirstPoint;
	}
	else
	{
		BreachWall.Reset();
	}
}

void AWB_SoccerAIController_Base::TickBreach(ACharacter* Me)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	AActor* Wall = BreachWall.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Wall)
	{
		BreachWall.Reset();
		PathPoints.Reset();
		return;
	}
	if (Now - BreachStartTime > Profile->BreachMaxDuration)
	{
		// Too long on this wall: give up, take the detour.
		IgnoredBreachWalls.Add(Wall, Now + Profile->BreachIgnoreDuration);
		BreachWall.Reset();
		PathPoints.Reset();
		return;
	}

	const FVector ToWall = (BreachPoint - Me->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const FVector Direction = ToWall.GetSafeNormal();
	const float Distance = static_cast<float>(ToWall.Size()) - Me->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Reach = Commands->GetDashDistanceForCharge(Profile->BotBreachDashCharge) * Profile->DashReachRatio;
	const float MinRunUp = Commands->GetDashMinRunUp() + Profile->DashRunUpMargin;

	// Run-up: far enough for the Dash to hurt the wall, close enough for it to reach.
	if (Distance > Reach)
	{
		Commands->MoveInWorldDirection(Direction);
		return;
	}
	if (Distance < MinRunUp)
	{
		Commands->MoveInWorldDirection(-Direction);
		return;
	}
	Commands->StopMoving();
	Commands->SetFacingDirection(Direction);
	if (CanDashNow())
	{
		NextBreachDashTime = Now + Profile->BreachDashCooldown;
		Commands->DashWithCharge(Profile->BotBreachDashCharge);
	}
}

bool AWB_SoccerAIController_Base::MoveTowardLocation(ACharacter* Me, const FVector& Target)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector Location = Me->GetActorLocation();

	if (FVector::Dist2D(Location, Target) <= Profile->AcceptanceRadius)
	{
		Commands->StopMoving();
		return true;
	}

	if (PathPoints.Num() < 2 || GetWorld()->GetTimeSeconds() >= NextRepathTime || FVector::Dist2D(Target, PathTarget) > Profile->RepathDistance)
	{
		RebuildPath(Me, Target);
	}

	// The straight line through the walls is faster than the detour: break the first wall.
	if (BreachWall.IsValid())
	{
		TickBreach(Me);
		return false;
	}

	while (PathIndex < PathPoints.Num() - 1 && FVector::Dist2D(Location, PathPoints[PathIndex]) <= Profile->WaypointRadius)
	{
		++PathIndex;
	}

	const FVector Next = PathPoints.IsValidIndex(PathIndex) ? PathPoints[PathIndex] : Target;
	Commands->MoveInWorldDirection(Next - Location);
	return false;
}

void AWB_SoccerAIController_Base::DrawDebug(const ACharacter* Me) const
{
#if ENABLE_DRAW_DEBUG
	if (!GetActiveProfile()->bDrawDebug)
	{
		return;
	}
	const UWorld* World = GetWorld();
	for (int32 i = 1; i < PathPoints.Num(); ++i)
	{
		DrawDebugLine(World, PathPoints[i - 1], PathPoints[i], i == PathIndex ? FColor::Yellow : FColor::Cyan, false, -1.f, 0, 3.f);
	}
	FString Text = FString::Printf(TEXT("Bot T%d %s"), GetTeamIndex(), *UEnum::GetDisplayValueAsText(CurrentIntent).ToString());
	if (CurrentRole != EWB_SoccerRole::None)
	{
		Text += FString::Printf(TEXT(" [%s]"), *UEnum::GetDisplayValueAsText(CurrentRole).ToString());
	}
	if (GetActiveProfile()->bUseFeatureFilter)
	{
		// Debug filter: show the intent refused this frame, so a still bot is not mistaken for a bug.
		Text += FilteredIntent != EWB_SoccerAIIntent::Idle
			? FString::Printf(TEXT("\nFilter: %s off"), *UEnum::GetDisplayValueAsText(FilteredIntent).ToString())
			: FString(TEXT("\nFilter: on"));
	}
	if (CurrentIntent == EWB_SoccerAIIntent::ChaseCarrier || CurrentIntent == EWB_SoccerAIIntent::Press)
	{
		// Dash check: reason, predicted spot of the carrier and real reach.
		Text += FString::Printf(TEXT("\nDash: %s"), *DashBlockReason.ToString());
		DrawDebugSphere(World, LastDashAimPoint, 30.f, 8, DashBlockReason == TEXT("Ready") ? FColor::Green : FColor::Red, false, -1.f, 0, 2.f);
		DrawDebugLine(World, Me->GetActorLocation(), LastDashAimPoint, FColor::Magenta, false, -1.f, 0, 1.f);
		if (Commands)
		{
			DrawDebugCircle(World, Me->GetActorLocation(), Commands->GetDashDistanceForCharge(GetActiveProfile()->BotDashCharge), 32, FColor::Magenta, false, -1.f, 0, 1.f, FVector::ForwardVector, FVector::RightVector, false);
		}
	}
	UWB_DebugTextSubsystem::DrawDebugText(World, Me->GetActorLocation() + FVector(0.f, 0.f, 140.f), Text, nullptr, FColor::Orange, 0.f, true);

	if (CurrentIntent == EWB_SoccerAIIntent::ReceivePass)
	{
		DrawDebugSphere(World, PassInterceptPoint, 20.f, 8, FColor::Cyan, false, -1.f, 0, 1.5f);
		DrawDebugLine(World, Me->GetActorLocation(), PassInterceptPoint, FColor::Cyan, false, -1.f, 0, 1.f);
	}

	if (const AActor* Wall = BreachWall.Get())
	{
		DrawDebugLine(World, Me->GetActorLocation(), BreachPoint, FColor::Red, false, -1.f, 0, 3.f);
		UWB_DebugTextSubsystem::DrawDebugText(World, BreachPoint + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("Breach %s"), *Wall->GetName()), nullptr, FColor::Red, 0.f, true);
	}

	if (CurrentRole == EWB_SoccerRole::Goalkeeper)
	{
		if (const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone())
		{
			const bool bStance = DiveComp && DiveComp->IsDefending();
			DrawDebugBox(World, Zone->GetComponentLocation(), Zone->GetScaledBoxExtent(), Zone->GetComponentQuat(), bStance ? FColor::Green : FColor::White, false, -1.f, 0, 2.f);
		}
	}

	if (const UWB_TeamGoalComponent* Goal = GetGoalToAttack())
	{
		const UWB_SoccerAIProfile* Profile = GetActiveProfile();
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMinDistance, 48, FColor::Red, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMaxDistance, 64, FColor::Green, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
	}
#endif
}
