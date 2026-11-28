// Goalkeeper brain of AWB_SoccerAIController_Base (devis gardien): behavior AI.Behavior.Goalkeeping.
// Threat branch: P1 reach the post, P2 placement and look, P3 stop the shot. No threat branch: shield of objects (patrol, fetch, place, check), supporter.
// Also the sight of every bot (AIPerception Sight) and its object memory.

#include "AI/WB_SoccerAIController_Base.h"

#include "AI/WB_SoccerAIProfile.h"
#include "AI/WB_SoccerAITags.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Debug/WB_DebugTextSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Grab/WB_GrabLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Match/WB_GoalkeeperZoneComponent.h"
#include "Match/WB_TeamGoalComponent.h"
#include "Match/WB_TeamZoneComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Player/Command/WB_PlayerCommandComponent.h"
#include "Player/Dive/WB_PlayerDiveComponent.h"
#include "Player/Kick/WB_KickProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"
#include "Player/WB_PlayerProfileLibrary.h"

namespace
{
	// Technical values (no gameplay effect): update cadences and tolerances of the bot brain.
	constexpr double MemoryUpdateInterval = 0.2;     // Object memory refresh from the sight.
	constexpr double GrabRetryInterval = 0.5;        // Time between two Grab presses on the same object.
	constexpr float GrabFacingTolerance = 20.f;      // Degrees: facing needed before a Grab or a throw.
	constexpr double ShieldRecheckInterval = 0.5;    // Time between two shield measures while the shield is in view.
	constexpr float MinShotApproachSpeed = 100.f;    // cm/s toward the goal under which a kicked ball is no shot.
	constexpr float ViewTraceSlack = 50.f;           // cm short of the point a view trace may stop and still see it.
	constexpr float DropSweepDepth = 2000.f;         // cm below the held object searched for its landing.

	bool IsHoldingBallState(const UWB_PlayerStateComponent* State)
	{
		return State && State->CurrentHolding.MatchesTag(WBTags::Player_Holding_Ball);
	}

	FVector Flat(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.f);
	}

	const TCHAR* KeeperTaskName(EWB_KeeperTask Task)
	{
		switch (Task)
		{
		case EWB_KeeperTask::Patrol:    return TEXT("Patrol");
		case EWB_KeeperTask::Fetch:     return TEXT("Fetch");
		case EWB_KeeperTask::Place:     return TEXT("Place");
		case EWB_KeeperTask::Settle:    return TEXT("Settle");
		case EWB_KeeperTask::Supporter: return TEXT("Supporter");
		case EWB_KeeperTask::Support:   return TEXT("Support");
		default:                        return TEXT("-");
		}
	}
}

// ---- Intent ----

EWB_SoccerAIIntent AWB_SoccerAIController_Base::ChooseGoalkeeperIntent()
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	ACharacter* Me = GetCharacter();
	if (!Profile || !Me || !PlayerStateComp)
	{
		return EWB_SoccerAIIntent::Idle;
	}

	// Caught the ball: clear it forward, still the Goalkeeper.
	if (IsHoldingBallState(PlayerStateComp))
	{
		return EWB_SoccerAIIntent::ClearBall;
	}
	const bool bHoldingObject = PlayerStateComp->IsHoldingAnything();

	// ---- Threat branch: stops the No threat branch at once (the held object is dropped by Tick) ----
	if (bKeeperThreat)
	{
		if (KeeperTask != EWB_KeeperTask::None)
		{
			SetKeeperTask(EWB_KeeperTask::None);
		}
		if (!bHoldingObject && RefreshIncomingPass() && IsIntentAllowed(EWB_SoccerAIIntent::ReceivePass))
		{
			return EWB_SoccerAIIntent::ReceivePass;
		}
		// P1: reach the post first, ignoring everything else.
		if (!IsAtGoalkeeperPost(Me))
		{
			return EWB_SoccerAIIntent::KeeperReturn;
		}
		// A free ball inside its zone: claim it (the stance catches it).
		if (!bHoldingObject && Profile->bGoalkeeperClaimsBallInZone && CachedBall.IsValid() && !FindBallCarrier())
		{
			const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone();
			if (Zone && Zone->IsLocationInside(CachedBall->GetActorLocation()))
			{
				if (IsIntentAllowed(EWB_SoccerAIIntent::SeekBall))
				{
					return EWB_SoccerAIIntent::SeekBall;
				}
				FilteredIntent = EWB_SoccerAIIntent::SeekBall;
			}
		}
		return EWB_SoccerAIIntent::GuardGoal;
	}

	// ---- No threat branch ----
	if (!bHoldingObject && RefreshIncomingPass() && IsIntentAllowed(EWB_SoccerAIIntent::ReceivePass))
	{
		return EWB_SoccerAIIntent::ReceivePass;
	}
	if (!IsIntentAllowed(EWB_SoccerAIIntent::KeeperPatrol))
	{
		// Debug filter: shield off, the Goalkeeper waits at its post.
		FilteredIntent = EWB_SoccerAIIntent::KeeperPatrol;
		return IsAtGoalkeeperPost(Me) ? EWB_SoccerAIIntent::Idle : EWB_SoccerAIIntent::KeeperReturn;
	}
	// Grab succeeded (or something is in the hands): carry it to the goal.
	if (bHoldingObject && KeeperTask != EWB_KeeperTask::Place && KeeperTask != EWB_KeeperTask::Settle)
	{
		SetKeeperTask(EWB_KeeperTask::Place);
	}
	if (KeeperTask == EWB_KeeperTask::None)
	{
		bool bCertified = false;
		MeasureShield(bCertified);
		SetKeeperTask(bCertified ? EWB_KeeperTask::Support : EWB_KeeperTask::Patrol);
	}

	switch (KeeperTask)
	{
	case EWB_KeeperTask::Patrol:    return EWB_SoccerAIIntent::KeeperPatrol;
	case EWB_KeeperTask::Fetch:     return EWB_SoccerAIIntent::KeeperFetch;
	case EWB_KeeperTask::Place:
	case EWB_KeeperTask::Settle:    return EWB_SoccerAIIntent::KeeperPlace;
	case EWB_KeeperTask::Supporter: return EWB_SoccerAIIntent::KeeperSupporter;
	case EWB_KeeperTask::Support:
		// Shield certified: the Goalkeeper supports its team like a Defender.
		return (TeamSituation == EWB_TeamSituation::Attacking && FindBallCarrier()) ? EWB_SoccerAIIntent::SupportOpen : EWB_SoccerAIIntent::Defend;
	default:
		return EWB_SoccerAIIntent::Idle;
	}
}

bool AWB_SoccerAIController_Base::ComputeThreat()
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const UWB_TeamZoneComponent* Zone = UWB_TeamZoneComponent::FindZoneForTeam(this, GetTeamIndex());
	if (!Profile || !Zone)
	{
		if (!bWarnedNoTeamZone)
		{
			bWarnedNoTeamZone = true;
			UE_LOG(LogTemp, Warning, TEXT("%s: no UWB_TeamZoneComponent for team %d in the level (BP_TeamZone expected). The Goalkeeper always sees a threat and stays in its goal."),
				*GetName(), GetTeamIndex());
		}
		return true;
	}

	const APawn* Carrier = FindBallCarrier();
	if (Carrier && IsOpponent(Carrier) && Zone->IsLocationInside(Carrier->GetActorLocation()))
	{
		return true;
	}
	const bool bAllyCarrierInZone = Carrier && !IsOpponent(Carrier) && Zone->IsLocationInside(Carrier->GetActorLocation());
	bool bOpponentInZone = false;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (*It == GetPawn() || !It->FindComponentByClass<UWB_PlayerStateComponent>() || !IsOpponent(*It))
		{
			continue;
		}
		const FVector Location = It->GetActorLocation();
		if (bAllyCarrierInZone && FVector::Dist2D(Location, Carrier->GetActorLocation()) <= Profile->ThreatNearAllyCarrierDistance)
		{
			return true;
		}
		bOpponentInZone |= Zone->IsLocationInside(Location);
	}
	const AActor* Ball = CachedBall.Get();
	return bOpponentInZone && Ball && Zone->IsLocationInside(Ball->GetActorLocation());
}

bool AWB_SoccerAIController_Base::IsAtGoalkeeperPost(const ACharacter* Me)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile || !Me)
	{
		return false;
	}
	const float Margin = Profile->GoalkeeperDistance + Profile->AcceptanceRadius + (bAtKeeperPost ? Profile->GoalkeeperZoneExitMargin : 0.f);
	if (const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone())
	{
		bAtKeeperPost = Zone->IsLocationInside(Me->GetActorLocation(), Margin);
	}
	else
	{
		const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
		bAtKeeperPost = OwnGoal && FVector::Dist2D(Me->GetActorLocation(), OwnGoal->GetAimLocation()) <= Margin;
	}
	return bAtKeeperPost;
}

// ---- Threat branch ----

FVector AWB_SoccerAIController_Base::GetGuardSpot(const FVector& ThreatLocation) const
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		return ThreatLocation;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector ToThreat = Flat(ThreatLocation - GoalLocation);
	const FVector Direction = ToThreat.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToThreat.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
	return GoalLocation + Direction * (GetPerimeterDistance(Direction) + Profile->GoalkeeperDistance) + Side * PositionError;
}

void AWB_SoccerAIController_Base::TickKeeperReturn(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		Commands->StopMoving();
		return;
	}
	FVector Threat;
	if (!GetThreatLocation(Threat))
	{
		Threat = OwnGoal->GetAimLocation() + OwnGoal->GetFieldDirection() * 1000.f;
	}
	// Sprint while far from the goal, walk (default speed) once close. After the move: the character starts a sprint only while it moves (RequestSprintStart).
	MoveTowardLocation(Me, GetGuardSpot(Threat));
	Commands->SetSprinting(FVector::Dist2D(Me->GetActorLocation(), OwnGoal->GetAimLocation()) > GetActiveProfile()->GoalkeeperRunDistance);
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
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector MyLocation = Me->GetActorLocation();

	// ReactionTime: the guard spot follows where the threat was ReactionTime seconds ago.
	FVector Threat;
	if (!GetThreatLocation(Threat))
	{
		Threat = OwnGoal->GetAimLocation() + OwnGoal->GetFieldDirection() * 1000.f;
	}
	ThreatHistory.Emplace(Now, Threat);
	const double ReactionStart = Now - Profile->ReactionTime;
	while (ThreatHistory.Num() > 1 && ThreatHistory[1].Key <= ReactionStart)
	{
		ThreatHistory.RemoveAt(0);
	}
	const FVector DelayedThreat = ThreatHistory[0].Value;

	// PositioningAccuracy: a side error drawn every PositionErrorInterval.
	if (Now >= NextPositionErrorTime)
	{
		NextPositionErrorTime = Now + Profile->PositionErrorInterval;
		PositionError = FMath::FRandRange(-1.f, 1.f) * (1.f - Profile->PositioningAccuracy) * Profile->PositionErrorMax;
	}

	// ---- P3: a shot interrupts P2 ----
	FVector BallLocation;
	FVector Cross;
	if (DetectShot(BallLocation, Cross))
	{
		if (!bShotActive)
		{
			bShotActive = true;
			ShotStartTime = Now;
			bDiveSent = false;
		}
		ShotBallLocation = BallLocation;
		ShotCross = Cross;

		// Point of the shot path closest to the Goalkeeper (flat).
		const FVector MeFlat = Flat(MyLocation);
		const FVector Closest = FMath::ClosestPointOnSegment(MeFlat, Flat(BallLocation), Flat(Cross));
		const float Lateral = FVector::Dist2D(MeFlat, Closest);

		const UWB_GoalkeeperZoneComponent* Zone = GetOwnGoalkeeperZone();
		const bool bCanDive = DiveComp && (!Profile->bUseFeatureFilter || Profile->bAllowDive) && !Commands->IsExhausted()
			&& (!Zone || Zone->IsLocationInside(MyLocation, Profile->GoalkeeperDistance + Profile->DiveMaxDistance));
		if (Lateral > Profile->ShotOnBodyDistance && !bDiveSent && bCanDive && Now - ShotStartTime >= Profile->DiveReactionTime)
		{
			// DiveAngle from the shot line: 90 = square to the shot, lower = also toward the shooter.
			const FVector ShotLine = (Flat(Cross) - Flat(BallLocation)).GetSafeNormal();
			const FVector TowardShooter = -ShotLine;
			FVector Side = Closest - MeFlat;
			Side -= FVector::DotProduct(Side, ShotLine) * ShotLine;
			Side = Side.GetSafeNormal();
			if (Side.IsNearlyZero())
			{
				Side = FVector::CrossProduct(FVector::UpVector, ShotLine);
			}
			const float AngleRad = FMath::DegreesToRadians(Profile->DiveAngle);
			FVector DiveDirection = (TowardShooter * FMath::Cos(AngleRad) + Side * FMath::Sin(AngleRad)).GetSafeNormal();
			const float Error = FMath::FRandRange(-1.f, 1.f) * (1.f - Profile->DiveAccuracy) * Profile->DiveErrorMax;
			DiveDirection = DiveDirection.RotateAngleAxis(Error, FVector::UpVector);

			bDiveSent = true;
			if (DiveComp->IsDefending())
			{
				DiveComp->StopDefense();
			}
			Commands->StopMoving();
			if (DiveComp->TryStartDiveToward(DiveDirection))
			{
				LastDiveDirection = DiveDirection;
				return;
			}
		}
		// On the path (or no dive): slide in the stance to stay on it.
		const FVector ToPath = Closest - MeFlat;
		if (ToPath.Size2D() > Profile->AcceptanceRadius * 0.5f)
		{
			Commands->MoveInWorldDirection(ToPath);
		}
		else
		{
			Commands->StopMoving();
		}
		KeeperFace(Me, BallLocation - MyLocation, DeltaSeconds);
		return;
	}
	bShotActive = false;

	// ---- P2: placement on the line threat - goal center, and look ----
	MoveTowardLocation(Me, GetGuardSpot(DelayedThreat));
	UpdateKeeperLook(Me, DeltaSeconds);
}

void AWB_SoccerAIController_Base::KeeperFace(ACharacter* Me, const FVector& Direction, float DeltaSeconds)
{
	const FVector FlatDirection = Flat(Direction).GetSafeNormal();
	if (FlatDirection.IsNearlyZero())
	{
		return;
	}
	if (DiveComp && DiveComp->IsDefending())
	{
		DiveComp->SetDefenseFacing(FlatDirection);
	}
	else
	{
		TurnToward(Me, FlatDirection, DeltaSeconds);
	}
}

void AWB_SoccerAIController_Base::UpdateKeeperLook(ACharacter* Me, float DeltaSeconds)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector MyLocation = Me->GetActorLocation();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	FVector LookAt = GoalLocation + OwnGoal->GetFieldDirection() * 1000.f;

	APawn* Carrier = FindBallCarrier();
	if (Carrier && Carrier != GetPawn())
	{
		// The look moves to a new carrier after the pass delay (UpdatePassChain).
		if (Carrier != LookCarrier.Get() && Now >= LookSwitchTime)
		{
			LookCarrier = Carrier;
		}
		LookAt = LookCarrier.IsValid() ? LookCarrier->GetActorLocation() : Carrier->GetActorLocation();
	}
	else if (const AActor* Ball = CachedBall.Get())
	{
		const FVector ToBall = Flat(Ball->GetActorLocation() - GoalLocation);
		const FVector Direction = ToBall.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToBall.GetSafeNormal();
		if (ToBall.Size() - GetPerimeterDistance(Direction) <= Profile->GoalkeeperLookFreeBallDistance)
		{
			LookAt = Ball->GetActorLocation();
		}
		else if (const UWB_TeamZoneComponent* Zone = UWB_TeamZoneComponent::FindZoneForTeam(this, GetTeamIndex()))
		{
			// Free ball far from the net: the nearest opponent of the zone.
			double BestDistSq = TNumericLimits<double>::Max();
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				if (*It != GetPawn() && It->FindComponentByClass<UWB_PlayerStateComponent>() && IsOpponent(*It) && Zone->IsLocationInside(It->GetActorLocation()))
				{
					const double DistSq = FVector::DistSquared2D(It->GetActorLocation(), MyLocation);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						LookAt = It->GetActorLocation();
					}
				}
			}
		}
	}
	KeeperFace(Me, LookAt - MyLocation, DeltaSeconds);
}

void AWB_SoccerAIController_Base::UpdatePassChain()
{
	APawn* Carrier = FindBallCarrier();
	if (!Carrier || !IsOpponent(Carrier))
	{
		// A teammate (or the Goalkeeper itself) took the ball: the chain of opponent passes is over.
		if (Carrier)
		{
			LastOpponentCarrier.Reset();
		}
		return;
	}
	if (Carrier == LastOpponentCarrier.Get())
	{
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastOpponentCarrier.IsValid())
	{
		// Pass between opponents: the look waits, longer for each pass chained within PassChainWindow.
		PassChainCount = (Now - LastPassTime <= Profile->PassChainWindow) ? PassChainCount + 1 : 1;
		LastPassTime = Now;
		const float Delay = FMath::Min(Profile->PassLookDelayBase + Profile->PassLookDelayStep * (PassChainCount - 1), Profile->PassLookDelayMax);
		LookSwitchTime = Now + Delay;
	}
	else
	{
		LookSwitchTime = Now;
	}
	LastOpponentCarrier = Carrier;
}

bool AWB_SoccerAIController_Base::DetectShot(FVector& OutBallLocation, FVector& OutCross) const
{
	const AActor* Ball = CachedBall.Get();
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!Ball || !OwnGoal || !UWB_BallPossessionSubsystem::IsBallKicked(this, Ball))
	{
		return false;
	}
	const UPrimitiveComponent* Body = UWB_BallPossessionSubsystem::FindBallBody(Ball);
	const FVector Location = Body ? Body->Bounds.Origin : Ball->GetActorLocation();
	const FVector Velocity = Body ? Body->GetComponentVelocity() : Ball->GetVelocity();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector FieldDirection = OwnGoal->GetFieldDirection();

	const float Approach = -FVector::DotProduct(Flat(Velocity), FieldDirection);
	const float Distance = FVector::DotProduct(Location - GoalLocation, FieldDirection);
	if (Approach < MinShotApproachSpeed || Distance < 0.f)
	{
		return false;
	}
	const float Time = Distance / Approach;
	const float GravityZ = (Body && Body->IsSimulatingPhysics() && Body->IsGravityEnabled()) ? GetWorld()->GetGravityZ() : 0.f;
	FVector Cross = Location + Velocity * Time;
	Cross.Z = Location.Z + Velocity.Z * Time + 0.5f * GravityZ * Time * Time;

	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const float Lateral = FVector::DotProduct(Cross - GoalLocation, OwnGoal->GetMouthAxis());
	const float Height = Cross.Z - GoalLocation.Z;
	if (FMath::Abs(Lateral) > OwnGoal->MouthHalfWidth + Profile->ShotMouthMargin || Height > OwnGoal->MouthHeight + Profile->ShotMouthMargin)
	{
		return false;
	}
	OutBallLocation = Location;
	OutCross = Cross;
	return true;
}

void AWB_SoccerAIController_Base::UpdateGoalkeeperStance(ACharacter* Me)
{
	if (!DiveComp)
	{
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const bool bDefending = DiveComp->IsDefending();
	// The stance belongs to the Threat branch, at the post: P2 / P3 and the claim of a free ball in the zone.
	const bool bGuarding = CurrentIntent == EWB_SoccerAIIntent::GuardGoal || CurrentIntent == EWB_SoccerAIIntent::SeekBall;
	const bool bWant = IsGoalkeeping() && Profile->bGoalkeeperAutoDefense && bKeeperThreat && bGuarding && IsAtGoalkeeperPost(Me);

	if (bWant && !bDefending && PlayerStateComp->IsInState(WBTags::Player_State_Free) && !PlayerStateComp->IsHoldingAnything())
	{
		DiveComp->TryStartDefense();
	}
	else if (!bWant && bDefending)
	{
		DiveComp->StopDefense();
	}
}

// ---- No threat branch ----

void AWB_SoccerAIController_Base::SetKeeperTask(EWB_KeeperTask NewTask)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	KeeperTask = NewTask;
	KeeperTaskStartTime = Now;
	PatrolElapsed = 0.f;
	NextGrabTryTime = 0.0;
	bThrowSent = false;
	bThrowPredicted = false;
	PathPoints.Reset();
	if (NewTask == EWB_KeeperTask::None)
	{
		KeeperTargetObject.Reset();
		KeeperTargetRank = -1;
		PlaceAttempts = 0;
	}
	if (NewTask == EWB_KeeperTask::Supporter)
	{
		const UWB_SoccerAIProfile* Profile = GetActiveProfile();
		NextRepatrolTime = Now + (Profile ? Profile->SupporterRepatrolInterval : 0.f);
	}
}

AActor* AWB_SoccerAIController_Base::FindShieldCandidate(int32 MinRank, int32 MaxRank) const
{
	const APawn* Me = GetPawn();
	const double Now = GetWorld()->GetTimeSeconds();
	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FWB_SeenObject& Seen : SeenObjects)
	{
		AActor* Actor = Seen.Actor.Get();
		if (!Actor || Seen.SizeRank < MinRank || Seen.SizeRank > MaxRank)
		{
			continue;
		}
		if (ShieldObjects.ContainsByPredicate([Actor](const FWB_ShieldObject& Entry) { return Entry.Actor.Get() == Actor; }))
		{
			continue;
		}
		if (const double* Until = SkippedShieldObjects.Find(Actor); Until && Now < *Until)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(Seen.Location, Me->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

void AWB_SoccerAIController_Base::TickKeeperPatrol(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		Commands->StopMoving();
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();

	auto StartFetch = [this](AActor* Target, int32 Rank)
	{
		KeeperTargetObject = Target;
		KeeperTargetRank = Rank;
		PlaceAttempts = 0;
		SetKeeperTask(EWB_KeeperTask::Fetch);
	};
	auto RankOf = [this](const AActor* Actor)
	{
		const FWB_SeenObject* Seen = SeenObjects.FindByPredicate([Actor](const FWB_SeenObject& Entry) { return Entry.Actor.Get() == Actor; });
		return Seen ? Seen->SizeRank : -1;
	};

	// A Large / XLarge object seen (now or before): fetch it at once.
	if (AActor* Large = FindShieldCandidate(3, 4))
	{
		StartFetch(Large, RankOf(Large));
		return;
	}

	// Back to the goal (the patrol starts there), then the look sweep around the field direction.
	const FVector FieldDirection = OwnGoal->GetFieldDirection();
	const FVector Spot = OwnGoal->GetAimLocation() + FieldDirection * (GetPerimeterDistance(FieldDirection) + Profile->GoalkeeperDistance);
	if (!MoveTowardLocation(Me, Spot))
	{
		return;
	}
	PatrolElapsed += DeltaSeconds;
	PatrolScanPhase += DeltaSeconds * Profile->PatrolScanSpeed / FMath::Max(Profile->PatrolScanHalfAngle, 1.f);
	const float Yaw = FieldDirection.Rotation().Yaw + Profile->PatrolScanHalfAngle * FMath::Sin(PatrolScanPhase);
	TurnToward(Me, FRotator(0.f, Yaw, 0.f).Vector(), DeltaSeconds);

	if (PatrolElapsed < Profile->PatrolDuration)
	{
		return;
	}
	// End of the patrol: the nearest Medium seen (two make a shield), else the Goalkeeper becomes a supporter.
	if (AActor* Medium = FindShieldCandidate(2, 2))
	{
		StartFetch(Medium, 2);
		return;
	}
	SetKeeperTask(EWB_KeeperTask::Supporter);
}

void AWB_SoccerAIController_Base::TickKeeperFetch(ACharacter* Me, float DeltaSeconds)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	AActor* Target = KeeperTargetObject.Get();
	if (!Target)
	{
		SetKeeperTask(EWB_KeeperTask::Patrol);
		return;
	}
	if (Now - KeeperTaskStartTime > Profile->ShieldTaskTimeout)
	{
		// Out of reach: skipped for a while, the patrol goes on.
		SkippedShieldObjects.Add(Target, Now + Profile->ObjectMemoryDuration);
		SetKeeperTask(EWB_KeeperTask::Patrol);
		return;
	}

	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(true, Origin, Extent);
	const FVector MyLocation = Me->GetActorLocation();
	const FVector Closest = FBox(Origin - Extent, Origin + Extent).GetClosestPointTo(MyLocation);
	const float Gap = FVector::Dist2D(MyLocation, Closest) - Me->GetCapsuleComponent()->GetScaledCapsuleRadius();
	if (Gap > Profile->ShieldGrabReach)
	{
		MoveTowardLocation(Me, Closest);
		return;
	}
	Commands->StopMoving();
	const float Remaining = TurnToward(Me, Flat(Origin - MyLocation).GetSafeNormal(), DeltaSeconds);
	if (Remaining <= GrabFacingTolerance && Now >= NextGrabTryTime)
	{
		NextGrabTryTime = Now + GrabRetryInterval;
		Commands->Grab();
	}
}

FVector AWB_SoccerAIController_Base::GetShieldTarget() const
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!OwnGoal)
	{
		return FVector::ZeroVector;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector FieldDirection = OwnGoal->GetFieldDirection();
	FVector Direction = FieldDirection;
	if (const AActor* Ball = CachedBall.Get())
	{
		const FVector ToBall = Flat(Ball->GetActorLocation() - GoalLocation).GetSafeNormal();
		if (FVector::DotProduct(ToBall, FieldDirection) > 0.f)
		{
			Direction = ToBall;
		}
	}
	FVector Target = GoalLocation + Direction * (GetPerimeterDistance(Direction) + Profile->ShieldDropOffset);
	if (KeeperTargetRank == 2)
	{
		// Two Medium objects side by side: the first one to one side, the second one to the other.
		const int32 MediumCount = ShieldObjects.FilterByPredicate([this](const FWB_ShieldObject& Entry)
		{
			return Entry.Actor.IsValid() && Entry.SizeRank == 2 && Entry.Actor.Get() != KeeperTargetObject.Get();
		}).Num();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
		Target += Side * (MediumCount == 0 ? -1.f : 1.f) * Profile->ShieldMediumSpacing;
	}
	return Target;
}

bool AWB_SoccerAIController_Base::PredictHeldObjectLanding(const ACharacter* Me, FVector& OutLanding) const
{
	const AActor* Held = KeeperTargetObject.Get();
	if (!Held)
	{
		return false;
	}
	FVector Origin;
	FVector Extent;
	Held->GetActorBounds(true, Origin, Extent);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBKeeperDropSweep), false, Me);
	Params.AddIgnoredActor(Held);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, DropSweepDepth), FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeBox(Extent * 0.9f), Params))
	{
		return false;
	}
	OutLanding = Hit.Location;
	return true;
}

void AWB_SoccerAIController_Base::TickKeeperPlace(ACharacter* Me, float DeltaSeconds)
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	AActor* Held = KeeperTargetObject.Get();

	if (KeeperTask == EWB_KeeperTask::Settle)
	{
		Commands->StopMoving();
		// Thrown: its path from the velocity after the release (debug).
		if (bThrowSent && !bThrowPredicted && Held && !PlayerStateComp->IsHoldingAnything())
		{
			const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Held->GetRootComponent());
			if (Body && Body->GetComponentVelocity().SizeSquared() > 1.f)
			{
				bThrowPredicted = true;
				FPredictProjectilePathParams PathParams(10.f, Body->Bounds.Origin, Body->GetComponentVelocity(), 3.f, ECC_Visibility, Held);
				PathParams.ActorsToIgnore.Add(Me);
				FPredictProjectilePathResult PathResult;
				UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
				ThrowPathPoints.Reset();
				for (const FPredictProjectilePathPointData& Point : PathResult.PathData)
				{
					ThrowPathPoints.Add(Point.Location);
				}
			}
		}
		if (Held)
		{
			TurnToward(Me, Flat(Held->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal(), DeltaSeconds);
		}
		if (Now >= SettleUntil && !PlayerStateComp->IsHoldingAnything())
		{
			EndShieldPlacement();
		}
		return;
	}

	if (!PlayerStateComp->IsHoldingAnything() || !Held)
	{
		// Lost on the way (hit, dropped): patrol again, the object is still known.
		SetKeeperTask(EWB_KeeperTask::Patrol);
		return;
	}
	if (Now - KeeperTaskStartTime > Profile->ShieldTaskTimeout)
	{
		Commands->Drop();
		SkippedShieldObjects.Add(Held, Now + Profile->ObjectMemoryDuration);
		SetKeeperTask(EWB_KeeperTask::Patrol);
		return;
	}

	const FVector Target = GetShieldTarget();
	FVector Landing;
	if (PredictHeldObjectLanding(Me, Landing) && FVector::Dist2D(Landing, Target) <= Profile->ShieldDropTolerance)
	{
		Commands->StopMoving();
		Commands->Drop();
		SettleUntil = Now + Profile->ShieldSettleTime;
		SetKeeperTask(EWB_KeeperTask::Settle);
		return;
	}

	// Target out of reach (no full path) or carried too long: throw it at the smallest charge that reaches.
	const bool bOutOfReach = (bLastPathPartial && PathPoints.Num() > 0) || Now - KeeperTaskStartTime > Profile->ShieldTaskTimeout * 0.5f;
	if (bOutOfReach && !bThrowSent)
	{
		Commands->StopMoving();
		const FVector ToTarget = Flat(Target - Me->GetActorLocation());
		const float Remaining = TurnToward(Me, ToTarget.GetSafeNormal(), DeltaSeconds);
		if (Remaining <= GrabFacingTolerance)
		{
			const float Charge = FMath::Clamp(static_cast<float>(ToTarget.Size()) / Profile->ShieldThrowFullChargeDistance, 0.f, 1.f);
			const UWB_KickProfile* KickProfile = UWB_PlayerProfileLibrary::GetPlayerKickProfile(Me);
			const float ChargePerSecond = KickProfile ? KickProfile->GetChargeStep(1.f) : 0.f;
			ChargeTarget = ChargePerSecond > 0.f ? Charge / ChargePerSecond : 0.f;
			ChargeElapsed = 0.f;
			bCharging = true;
			bChargeFacingOverride = true;
			ChargeFacing = ToTarget.GetSafeNormal();
			Commands->KickPress();
			// Settle directly (not SetKeeperTask, which clears bThrowSent): the charge is released by TickCharge, then the object flies and settles.
			bThrowSent = true;
			SettleUntil = Now + ChargeTarget + Profile->ShieldSettleTime;
			KeeperTask = EWB_KeeperTask::Settle;
		}
		return;
	}
	// The object is carried in front: walk so that the object (not the Goalkeeper) reaches the target.
	FVector HeldOrigin;
	FVector HeldExtent;
	Held->GetActorBounds(true, HeldOrigin, HeldExtent);
	MoveTowardLocation(Me, Target - Flat(HeldOrigin - Me->GetActorLocation()));
}

void AWB_SoccerAIController_Base::EndShieldPlacement()
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	AActor* Placed = KeeperTargetObject.Get();
	bool bCertified = false;
	MeasureShield(bCertified);

	if (Placed)
	{
		FWB_ShieldObject* Entry = ShieldObjects.FindByPredicate([Placed](const FWB_ShieldObject& Item) { return Item.Actor.Get() == Placed; });
		if (!Entry)
		{
			Entry = &ShieldObjects.AddDefaulted_GetRef();
			Entry->Actor = Placed;
		}
		Entry->PlacedLocation = Placed->GetActorLocation();
		Entry->SizeRank = KeeperTargetRank;
		Entry->MissingSince = 0.0;
	}
	if (bCertified)
	{
		SetKeeperTask(EWB_KeeperTask::Support);
		return;
	}

	++PlaceAttempts;
	const int32 MediumCount = ShieldObjects.FilterByPredicate([](const FWB_ShieldObject& Item) { return Item.Actor.IsValid() && Item.SizeRank == 2; }).Num();
	if (KeeperTargetRank == 2 && MediumCount < 2)
	{
		// First Medium in place: patrol for the second one.
		SetKeeperTask(EWB_KeeperTask::Patrol);
		KeeperTargetObject.Reset();
		PlaceAttempts = 0;
		return;
	}
	if (Placed && PlaceAttempts < Profile->ShieldMaxAttempts)
	{
		// Coverage too low: fetch the same object again and place it better.
		SetKeeperTask(EWB_KeeperTask::Fetch);
		return;
	}
	// Given up: the goal stays unprotected, the Goalkeeper becomes a supporter.
	if (Placed)
	{
		SkippedShieldObjects.Add(Placed, Now + Profile->ObjectMemoryDuration);
		ShieldObjects.RemoveAll([Placed](const FWB_ShieldObject& Item) { return Item.Actor.Get() == Placed; });
	}
	KeeperTargetObject.Reset();
	PlaceAttempts = 0;
	SetKeeperTask(EWB_KeeperTask::Supporter);
}

void AWB_SoccerAIController_Base::TickKeeperSupporter(ACharacter* Me, float DeltaSeconds)
{
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	const AActor* Ball = CachedBall.Get();
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (GetWorld()->GetTimeSeconds() >= NextRepatrolTime)
	{
		// Every SupporterRepatrolInterval: still unprotected, patrol again (new objects may be in view).
		bool bCertified = false;
		MeasureShield(bCertified);
		SetKeeperTask(bCertified ? EWB_KeeperTask::Support : EWB_KeeperTask::Patrol);
		return;
	}
	if (!OwnGoal || !Ball)
	{
		Commands->StopMoving();
		return;
	}
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector ToBall = Flat(Ball->GetActorLocation() - GoalLocation);
	const FVector Direction = ToBall.IsNearlyZero() ? OwnGoal->GetFieldDirection() : ToBall.GetSafeNormal();
	// Between the ball and the goal, SupporterMinDistance to SupporterMaxDistance from the goal, never past the ball.
	const float BallSide = FMath::Max(static_cast<float>(ToBall.Size()) - Profile->SupporterGapToBall, 0.f);
	const float Distance = FMath::Min(FMath::Clamp(BallSide, Profile->SupporterMinDistance, Profile->SupporterMaxDistance), BallSide);
	HoldSpot(Me, GoalLocation + Direction * Distance, Ball->GetActorLocation(), DeltaSeconds);
}

void AWB_SoccerAIController_Base::CheckShieldMoved(const ACharacter* Me)
{
	if (ShieldObjects.IsEmpty() || !Perception)
	{
		return;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<AActor*> Seen;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);
	const bool bHandlingTarget = KeeperTask == EWB_KeeperTask::Fetch || KeeperTask == EWB_KeeperTask::Place || KeeperTask == EWB_KeeperTask::Settle;

	bool bMoved = false;
	bool bShieldInView = false;
	for (int32 Index = ShieldObjects.Num() - 1; Index >= 0; --Index)
	{
		FWB_ShieldObject& Entry = ShieldObjects[Index];
		AActor* Actor = Entry.Actor.Get();
		if (bHandlingTarget && Actor && Actor == KeeperTargetObject.Get())
		{
			continue; // The object it is moving itself.
		}
		bool bThisMoved = false;
		if (!Actor)
		{
			bThisMoved = true;
		}
		else if (Seen.Contains(Actor))
		{
			// Seen: moved when it is away from its place.
			bThisMoved = FVector::Dist(Actor->GetActorLocation(), Entry.PlacedLocation) > Profile->ShieldMovedDistance;
			Entry.MissingSince = 0.0;
			bShieldInView |= !bThisMoved;
		}
		else if (IsLocationInView(Me, Entry.PlacedLocation))
		{
			// Its place is in view without it: gone after ShieldMissingConfirmTime.
			if (Entry.MissingSince <= 0.0)
			{
				Entry.MissingSince = Now;
			}
			bThisMoved = Now - Entry.MissingSince >= Profile->ShieldMissingConfirmTime;
		}
		else
		{
			Entry.MissingSince = 0.0;
		}
		if (bThisMoved)
		{
			ShieldObjects.RemoveAt(Index);
			bMoved = true;
		}
	}

	if (bMoved)
	{
		bShieldCertified = false;
		// No threat: back to the goal and patrol (the patrol starts at the goal). With a threat, the Threat branch already runs.
		if (!bKeeperThreat && (KeeperTask == EWB_KeeperTask::Support || KeeperTask == EWB_KeeperTask::Supporter))
		{
			SetKeeperTask(EWB_KeeperTask::Patrol);
		}
		return;
	}
	// The shield is in view: measure it again (it may have been pushed a little).
	if (bShieldInView && KeeperTask == EWB_KeeperTask::Support && Now >= NextShieldMeasureTime)
	{
		NextShieldMeasureTime = Now + ShieldRecheckInterval;
		bool bCertified = false;
		MeasureShield(bCertified);
		if (!bCertified && !bKeeperThreat)
		{
			SetKeeperTask(EWB_KeeperTask::Patrol);
		}
	}
}

float AWB_SoccerAIController_Base::MeasureShield(bool& OutCertified)
{
	OutCertified = false;
	ShieldRays.Reset();
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	const UWB_TeamGoalComponent* OwnGoal = GetGoalToDefend();
	if (!Profile || !OwnGoal)
	{
		ShieldCoverage = 0.f;
		bShieldCertified = false;
		return 0.f;
	}
	const FVector GoalLocation = OwnGoal->GetAimLocation();
	const FVector FieldDirection = OwnGoal->GetFieldDirection();
	const FVector Axis = OwnGoal->GetMouthAxis();
	const float HalfWidth = OwnGoal->MouthHalfWidth;
	const float Height = OwnGoal->MouthHeight;
	const int32 Columns = FMath::Max(1, Profile->ShieldGridColumns);
	const int32 Rows = FMath::Max(1, Profile->ShieldGridRows);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBKeeperShieldGrid), false, GetPawn());

	TSet<const AActor*> LargeBlockers;
	TSet<const AActor*> MediumBlockers;
	int32 Blocked = 0;
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		// Columns across the width, rows at the middle of equal bands of the height (never on the floor itself).
		const float U = Columns > 1 ? FMath::Lerp(-1.f, 1.f, static_cast<float>(Column) / (Columns - 1)) : 0.f;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float V = (Row + 0.5f) / Rows;
			const FVector Start = GoalLocation + Axis * (U * HalfWidth) + FVector::UpVector * (V * Height);
			const FVector End = Start + FieldDirection * Profile->ShieldTraceLength;
			TArray<FHitResult> Hits;
			GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, Params);
			bool bRayBlocked = false;
			for (const FHitResult& Hit : Hits)
			{
				AActor* HitActor = Hit.GetActor();
				if (!HitActor || Cast<APawn>(HitActor))
				{
					continue;
				}
				// Only shield objects count: Medium or bigger.
				const int32 Rank = GetCachedSizeRank(HitActor);
				if (Rank >= 2)
				{
					bRayBlocked = true;
					(Rank >= 3 ? LargeBlockers : MediumBlockers).Add(HitActor);
					break;
				}
			}
			Blocked += bRayBlocked ? 1 : 0;
			ShieldRays.Add({ Start, End, bRayBlocked });
		}
	}
	ShieldCoverage = static_cast<float>(Blocked) / (Columns * Rows);
	OutCertified = ShieldCoverage >= Profile->ShieldCoverageThreshold && (LargeBlockers.Num() > 0 || MediumBlockers.Num() >= 2);
	bShieldCertified = OutCertified;
	return ShieldCoverage;
}

// ---- Sight and object memory (all bots) ----

void AWB_SoccerAIController_Base::ApplySightSettings()
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile || !Perception || !SightConfig)
	{
		return;
	}
	const FWB_BotSightSettings* Settings = Profile->RoleSight.Find(CurrentRole);
	if (!Settings)
	{
		Settings = &Profile->DefaultSight;
	}
	const float LoseRadius = FMath::Max(Settings->LoseSightRadius, Settings->SightRadius);
	if (Settings->SightRadius == AppliedSightRadius && LoseRadius == AppliedLoseSightRadius && Settings->PeripheralHalfAngle == AppliedSightHalfAngle)
	{
		return;
	}
	AppliedSightRadius = Settings->SightRadius;
	AppliedLoseSightRadius = LoseRadius;
	AppliedSightHalfAngle = Settings->PeripheralHalfAngle;
	SightConfig->SightRadius = AppliedSightRadius;
	SightConfig->LoseSightRadius = AppliedLoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = AppliedSightHalfAngle;
	Perception->ConfigureSense(*SightConfig);
	Perception->RequestStimuliListenerUpdate();
}

bool AWB_SoccerAIController_Base::IsLocationInView(const ACharacter* Me, const FVector& Location) const
{
	FVector Eyes;
	FRotator EyesRotation;
	GetActorEyesViewPoint(Eyes, EyesRotation);
	const FVector ToLocation = Location - Eyes;
	const float Distance = ToLocation.Size();
	if (Distance > AppliedSightRadius || Distance < KINDA_SMALL_NUMBER)
	{
		return Distance < KINDA_SMALL_NUMBER;
	}
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(AppliedSightHalfAngle));
	if (FVector::DotProduct(EyesRotation.Vector(), ToLocation / Distance) < CosHalf)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBBotViewTrace), false, Me);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Location, ECC_Visibility, Params))
	{
		return Hit.Distance >= Distance - ViewTraceSlack;
	}
	return true;
}

int32 AWB_SoccerAIController_Base::GetCachedSizeRank(AActor* Actor) const
{
	if (!Actor)
	{
		return -1;
	}
	if (const int32* Cached = SizeRankCache.Find(Actor))
	{
		return *Cached;
	}
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	int32 Rank = -1;
	// NoGrab first: an excluded object is never even sized.
	if (Profile && !UWB_GrabLibrary::IsGrabExcluded(Actor, Profile->NoGrabTag))
	{
		Rank = UWB_GrabLibrary::GetObjectSizeRank(UWB_GrabLibrary::GetObjectSizeTag(Actor, Profile->PhysicsProfileFunction, Profile->SizeTagVariable));
	}
	SizeRankCache.Add(Actor, Rank);
	return Rank;
}

void AWB_SoccerAIController_Base::UpdateObjectMemory()
{
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Perception || !Profile)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMemoryUpdateTime)
	{
		return;
	}
	NextMemoryUpdateTime = Now + MemoryUpdateInterval;

	TArray<AActor*> Seen;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);
	for (AActor* Actor : Seen)
	{
		// Players are read directly by the brain; the memory keeps the objects and the walls.
		if (!Actor || Cast<APawn>(Actor))
		{
			continue;
		}
		FWB_SeenObject* Entry = SeenObjects.FindByPredicate([Actor](const FWB_SeenObject& Item) { return Item.Actor.Get() == Actor; });
		if (!Entry)
		{
			Entry = &SeenObjects.AddDefaulted_GetRef();
			Entry->Actor = Actor;
			Entry->SizeRank = GetCachedSizeRank(Actor);
		}
		Entry->Location = Actor->GetActorLocation();
		Entry->LastSeenTime = Now;
	}
	SeenObjects.RemoveAll([Now, Profile](const FWB_SeenObject& Item)
	{
		return !Item.Actor.IsValid() || Now - Item.LastSeenTime > Profile->ObjectMemoryDuration;
	});
	for (auto It = SizeRankCache.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = SkippedShieldObjects.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || Now >= It->Value)
		{
			It.RemoveCurrent();
		}
	}
}

// ---- Debug ----

void AWB_SoccerAIController_Base::DrawGoalkeeperDebug(const ACharacter* Me) const
{
#if ENABLE_DRAW_DEBUG
	const UWB_SoccerAIProfile* Profile = GetActiveProfile();
	if (!Profile || !Profile->bDrawGoalkeeperDebug || !IsGoalkeeping())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const FString Text = FString::Printf(TEXT("GK %s | %s | shield %d%%%s"), bKeeperThreat ? TEXT("THREAT") : TEXT("calm"),
		bKeeperThreat ? *UEnum::GetDisplayValueAsText(CurrentIntent).ToString() : KeeperTaskName(KeeperTask),
		FMath::RoundToInt(ShieldCoverage * 100.f), bShieldCertified ? TEXT(" OK") : TEXT(""));
	UWB_DebugTextSubsystem::DrawDebugText(World, Me->GetActorLocation() + FVector(0.f, 0.f, 190.f), Text, nullptr, bKeeperThreat ? FColor::Red : FColor::Cyan, 0.f, true);

	if (const UWB_TeamZoneComponent* Zone = UWB_TeamZoneComponent::FindZoneForTeam(this, GetTeamIndex()))
	{
		DrawDebugBox(World, Zone->GetComponentLocation(), Zone->GetScaledBoxExtent(), Zone->GetComponentQuat(), bKeeperThreat ? FColor::Red : FColor::Blue, false, -1.f, 0, 3.f);
	}
	for (const FShieldRay& Ray : ShieldRays)
	{
		DrawDebugLine(World, Ray.Start, Ray.End, Ray.bBlocked ? FColor::Green : FColor::Red, false, -1.f, 0, 1.5f);
	}
	for (const FWB_ShieldObject& Entry : ShieldObjects)
	{
		DrawDebugSphere(World, Entry.PlacedLocation, 40.f, 8, Entry.MissingSince > 0.0 ? FColor::Orange : FColor::Yellow, false, -1.f, 0, 2.f);
	}
	for (const FWB_SeenObject& Seen : SeenObjects)
	{
		if (Seen.SizeRank >= 2)
		{
			DrawDebugPoint(World, Seen.Location + FVector(0.f, 0.f, 60.f), 12.f, Seen.SizeRank >= 3 ? FColor::Magenta : FColor::Purple, false, -1.f);
		}
	}
	if (const AActor* Target = KeeperTargetObject.Get())
	{
		DrawDebugLine(World, Me->GetActorLocation(), Target->GetActorLocation(), FColor::Yellow, false, -1.f, 0, 2.f);
		if (KeeperTask == EWB_KeeperTask::Place || KeeperTask == EWB_KeeperTask::Settle)
		{
			DrawDebugSphere(World, GetShieldTarget(), Profile->ShieldDropTolerance, 12, FColor::White, false, -1.f, 0, 1.5f);
		}
	}
	for (int32 Index = 1; Index < ThrowPathPoints.Num(); ++Index)
	{
		DrawDebugLine(World, ThrowPathPoints[Index - 1], ThrowPathPoints[Index], FColor::Orange, false, -1.f, 0, 2.f);
	}
	if (bShotActive)
	{
		DrawDebugLine(World, ShotBallLocation, ShotCross, FColor::Magenta, false, -1.f, 0, 3.f);
		DrawDebugSphere(World, ShotCross, 25.f, 8, FColor::Magenta, false, -1.f, 0, 2.f);
	}
	if (!LastDiveDirection.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Me->GetActorLocation(), Me->GetActorLocation() + LastDiveDirection * 200.f, 30.f, FColor::Orange, false, -1.f, 0, 3.f);
	}
#endif
}
