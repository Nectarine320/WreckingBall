#include "AI/WB_SoccerAIController_Base.h"

#include "AI/WB_SoccerAIProfile.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Match/WB_TeamGoalComponent.h"
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
	return AIProfile ? AIProfile.Get() : GetDefault<UWB_SoccerAIProfile>();
}

int32 AWB_SoccerAIController_Base::GetTeamIndex() const
{
	return PlayerHit ? PlayerHit->TeamIndex : -1;
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
	const UWB_PlayerHitComponent* OtherHit = Other->FindComponentByClass<UWB_PlayerHitComponent>();
	const int32 MyTeam = GetTeamIndex();
	return !OtherHit || MyTeam < 0 || OtherHit->TeamIndex < 0 || OtherHit->TeamIndex != MyTeam;
}

UWB_TeamGoalComponent* AWB_SoccerAIController_Base::GetGoalToAttack() const
{
	const APawn* Me = GetPawn();
	return Me ? UWB_TeamGoalComponent::FindGoalToAttack(this, GetTeamIndex(), Me->GetActorLocation()) : nullptr;
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
	if (!Me || !Commands || !PlayerStateComp)
	{
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
		// The grab sphere sits in front of the player: face the ball, then grab.
		Commands->StopMoving();
		Commands->SetFacingDirection(ToBall);
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextGrabTime)
		{
			NextGrabTime = Now + Profile->GrabRetryInterval;
			Commands->Grab();
		}
		return;
	}
	MoveTowardLocation(Me, Ball->GetActorLocation());
}

void AWB_SoccerAIController_Base::TickChaseCarrier(ACharacter* Me, float DeltaSeconds)
{
	const APawn* Carrier = FindBallCarrier();
	if (!Carrier)
	{
		Commands->StopMoving();
		return;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector ToCarrier = Carrier->GetActorLocation() - Me->GetActorLocation();
	const float Distance = ToCarrier.Size2D();
	const double Now = GetWorld()->GetTimeSeconds();

	if (Distance >= Profile->DashMinDistance && Distance <= Profile->DashMaxDistance && Now >= NextDashTime)
	{
		// Dash along the actor forward: face the carrier with a random error, so some Dashes miss.
		NextDashTime = Now + Profile->DashCooldown;
		const float Jitter = FMath::FRandRange(-Profile->DashAngleJitter, Profile->DashAngleJitter);
		Commands->StopMoving();
		Commands->SetFacingDirection(ToCarrier.GetSafeNormal2D().RotateAngleAxis(Jitter, FVector::UpVector));
		Commands->Dash();
		return;
	}
	MoveTowardLocation(Me, Carrier->GetActorLocation());
}

void AWB_SoccerAIController_Base::TickSupport(ACharacter* Me, float DeltaSeconds)
{
	const APawn* Carrier = FindBallCarrier();
	const UWB_TeamGoalComponent* Goal = GetGoalToAttack();
	if (!Carrier || !Goal)
	{
		Commands->StopMoving();
		return;
	}

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector CarrierLocation = Carrier->GetActorLocation();
	const FVector Forward = (Goal->GetAimLocation() - CarrierLocation).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	// Stay on the side the bot is already on.
	const float SideSign = FVector::DotProduct(Me->GetActorLocation() - CarrierLocation, Side) >= 0.f ? 1.f : -1.f;
	const FVector Spot = CarrierLocation + Forward * Profile->SupportLeadDistance + Side * SideSign * Profile->SupportLateralOffset;
	MoveTowardLocation(Me, Spot);
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
		const FVector SpotDir = ClampDirectionToCone(FromGoalDir.IsNearlyZero() ? FieldDirection : FromGoalDir, FieldDirection, Profile->ShootConeHalfAngle * 0.8f);
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
	const FString Text = FString::Printf(TEXT("Bot T%d %s"), GetTeamIndex(), *UEnum::GetDisplayValueAsText(CurrentIntent).ToString());
	DrawDebugString(World, Me->GetActorLocation() + FVector(0.f, 0.f, 140.f), Text, nullptr, FColor::Orange, 0.f, true);

	if (const UWB_TeamGoalComponent* Goal = GetGoalToAttack())
	{
		const UWB_SoccerAIProfile* Profile = GetActiveProfile();
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMinDistance, 48, FColor::Red, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
		DrawDebugCircle(World, Goal->GetAimLocation(), Profile->ShootMaxDistance, 64, FColor::Green, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
	}
#endif
}
