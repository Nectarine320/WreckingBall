#include "AI/WB_SoccerAIController_Base.h"

#include "AI/WB_SoccerAIProfile.h"
#include "AI/Team/WB_TeamCoordinatorComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Match/WB_TeamGoalComponent.h"
#include "Match/WB_TeamLibrary.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Player/Command/WB_PlayerCommandComponent.h"
#include "Player/Hit/WB_PlayerHitComponent.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

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

void AWB_SoccerAIController_Base::SetTeamRole(EWB_SoccerRole InRole, EWB_TeamSituation InSituation, APawn* InPressTarget)
{
	CurrentRole = InRole;
	TeamSituation = InSituation;
	PressTarget = InPressTarget;
}

APawn* AWB_SoccerAIController_Base::FindBallCarrier() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (IsHoldingBall(It->FindComponentByClass<UWB_PlayerStateComponent>()))
		{
			return *It;
		}
	}
	return nullptr;
}

void AWB_SoccerAIController_Base::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	PlayerStateComp = InPawn ? InPawn->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	PlayerHit = InPawn ? InPawn->FindComponentByClass<UWB_PlayerHitComponent>() : nullptr;
	Commands = InPawn ? InPawn->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
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
	PlayerStateComp = nullptr;
	PlayerHit = nullptr;
	Commands = nullptr;
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
	CurrentIntent = EWB_SoccerAIIntent::Idle;
}

EWB_SoccerAIIntent AWB_SoccerAIController_Base::ChooseIntent_Implementation()
{
	if (PlayerStateComp && PlayerStateComp->IsHoldingAnything())
	{
		return EWB_SoccerAIIntent::Attack;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (Profile && Profile->bUseTeamRoles && CurrentRole != EWB_SoccerRole::None)
	{
		if (CurrentRole != EWB_SoccerRole::Attacker && PressTarget.IsValid())
		{
			return EWB_SoccerAIIntent::Press;
		}
		const bool bTeamHasBall = TeamSituation == EWB_TeamSituation::Attacking;
		switch (CurrentRole)
		{
		case EWB_SoccerRole::Defender:
			return bTeamHasBall ? EWB_SoccerAIIntent::Support : EWB_SoccerAIIntent::Defend;
		case EWB_SoccerRole::Goalkeeper:
			// The team holds the ball: the Goalkeeper becomes a defender only once the ball is in enemy territory.
			return (bTeamHasBall && IsBallInEnemyTerritory()) ? EWB_SoccerAIIntent::LastMan : EWB_SoccerAIIntent::GuardGoal;
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
	if (!PlayerStateComp->IsInState(WBTags::Player_State_Free))
	{
		Commands->StopMoving();
		return;
	}

	const EWB_SoccerAIIntent NewIntent = ChooseIntent();
	if (NewIntent != CurrentIntent)
	{
		const EWB_SoccerAIIntent OldIntent = CurrentIntent;
		CurrentIntent = NewIntent;
		PathPoints.Reset();
		bAimJitterDrawn = false;
		OnIntentChanged(OldIntent, NewIntent);
	}

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

	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(GetWorld(), BallClass); It; ++It)
	{
		const double DistSq = FVector::DistSquared(It->GetActorLocation(), Me->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	CachedBall = Best;
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
	if (ToBall.Size2D() <= Profile->GrabRange)
	{
		// Close: walk straight into the ball, the catch zone in front of the player takes it automatically.
		Commands->MoveInWorldDirection(ToBall);
		return;
	}
	MoveTowardLocation(Me, Ball->GetActorLocation());
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
	// Dash along the actor forward: face the predicted spot with a random error, so some Dashes miss.
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	NextDashTime = GetWorld()->GetTimeSeconds() + Profile->DashCooldown;
	const float Jitter = FMath::FRandRange(-Profile->DashAngleJitter, Profile->DashAngleJitter);
	Commands->StopMoving();
	Commands->SetFacingDirection((AimPoint - Me->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(Jitter, FVector::UpVector));
	Commands->Dash();
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
	const float DashDistance = Commands->GetDashDistance();
	const float DashSpeed = DashDistance / Commands->GetDashDuration();
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
	if (MoveTowardLocation(Me, Spot))
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

	// Defensive zone: half the distance between the two goals (times DefendRadiusScale), around the own goal.
	float Radius = TNumericLimits<float>::Max();
	if (const UWB_TeamGoalComponent* OtherGoal = GetGoalToAttack())
	{
		Radius = FVector::Dist2D(GoalLocation, OtherGoal->GetAimLocation()) * 0.5f * Profile->DefendRadiusScale;
	}

	// On the line threat - goal, DefendGapToBall short of the threat, never out of the zone.
	const FVector ToThreat = (Threat - GoalLocation) * FVector(1.f, 1.f, 0.f);
	const float Distance = FMath::Clamp(static_cast<float>(ToThreat.Size()) - Profile->DefendGapToBall, 0.f, Radius);
	const FVector Direction = ToThreat.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToThreat.GetSafeNormal();
	HoldSpot(Me, GoalLocation + Direction * Distance, Threat, DeltaSeconds);
}

void AWB_SoccerAIController_Base::TickGuardGoal(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		Commands->StopMoving();
		return;
	}
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	FVector Threat;
	const bool bHasThreat = GetThreatLocation(Threat);
	const FVector ToThreat = bHasThreat ? (Threat - GoalLocation).GetSafeNormal2D() : FVector::ZeroVector;
	const FVector Direction = ToThreat.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToThreat;
	HoldSpot(Me, GoalLocation + Direction * GetActiveProfile()->GoalkeeperDistance, bHasThreat ? Threat : GoalLocation + Direction * 1000.f, DeltaSeconds);
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
	Distance = FMath::Max(Distance, Profile->GoalkeeperDistance);
	const FVector Direction = ToBall.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToBall.GetSafeNormal();
	HoldSpot(Me, GoalLocation + Direction * Distance, Ball->GetActorLocation(), DeltaSeconds);
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
	if (Remaining <= Profile->ShootAimTolerance)
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
		if (Result.IsSuccessful() && Result.Path.IsValid())
		{
			for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
			{
				PathPoints.Add(Point.Location);
			}
		}
	}

	// No NavMesh or no path: straight line.
	if (PathPoints.Num() < 2)
	{
		PathPoints = { Start, Target };
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
	if (CurrentIntent == EWB_SoccerAIIntent::ChaseCarrier || CurrentIntent == EWB_SoccerAIIntent::Press)
	{
		// Dash check: reason, predicted spot of the carrier and real reach.
		Text += FString::Printf(TEXT("\nDash: %s"), *DashBlockReason.ToString());
		DrawDebugSphere(World, LastDashAimPoint, 30.f, 8, DashBlockReason == TEXT("Ready") ? FColor::Green : FColor::Red, false, -1.f, 0, 2.f);
		DrawDebugLine(World, Me->GetActorLocation(), LastDashAimPoint, FColor::Magenta, false, -1.f, 0, 1.f);
		if (Commands)
		{
			DrawDebugCircle(World, Me->GetActorLocation(), Commands->GetDashDistance(), 32, FColor::Magenta, false, -1.f, 0, 1.f, FVector::ForwardVector, FVector::RightVector, false);
		}
	}
	DrawDebugString(World, Me->GetActorLocation() + FVector(0.f, 0.f, 140.f), Text, nullptr, FColor::Orange, 0.f, true);

	if (const UWB_TeamGoalComponent* Goal = GetGoalToAttack())
	{
		const UWB_SoccerAIProfile* Profile = GetActiveProfile();
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMinDistance, 48, FColor::Red, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMaxDistance, 64, FColor::Green, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
	}
#endif
}
