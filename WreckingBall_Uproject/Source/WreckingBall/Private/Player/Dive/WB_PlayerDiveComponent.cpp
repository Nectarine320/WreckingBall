#include "Player/Dive/WB_PlayerDiveComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Player/Dive/WB_PlayerDiveProfile.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerDive, Log, All);

namespace
{
	float AnimLengthOr(const UAnimSequenceBase* Anim, float Fallback)
	{
		// Real playing time: the asset's RateScale speeds it up or slows it down.
		return Anim ? Anim->GetPlayLength() / FMath::Max(FMath::Abs(Anim->RateScale), 0.01f) : Fallback;
	}
}

UWB_PlayerDiveComponent::UWB_PlayerDiveComponent()
{
	// Ticks only during a dive or the defense stance.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWB_PlayerDiveComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		PlayerState = Owner->FindComponentByClass<UWB_PlayerStateComponent>();
	}
	if (PlayerState)
	{
		PlayerState->OnPlayerStateChanged.AddDynamic(this, &UWB_PlayerDiveComponent::HandlePlayerStateChanged);
	}
	else
	{
		UE_LOG(LogWBPlayerDive, Warning, TEXT("%s: UWB_PlayerDiveComponent needs a UWB_PlayerStateComponent on the same actor."), *GetNameSafe(GetOwner()));
	}
}

const UWB_PlayerDiveProfile* UWB_PlayerDiveComponent::GetActiveDiveProfile() const
{
	if (!DiveProfile && !bWarnedNoProfile)
	{
		bWarnedNoProfile = true;
		UE_LOG(LogWBPlayerDive, Warning, TEXT("%s: no DiveProfile on %s (DA_PlayerDive_Default expected). The dive and the defense are refused."), *GetNameSafe(GetOwner()), *GetName());
	}
	return DiveProfile.Get();
}

UPrimitiveComponent* UWB_PlayerDiveComponent::FindBallBody(AActor* Ball)
{
	if (!IsValid(Ball))
	{
		return nullptr;
	}
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Ball->GetRootComponent()); Root && Root->IsSimulatingPhysics())
	{
		return Root;
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Ball);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsSimulatingPhysics())
		{
			return Primitive;
		}
	}
	return nullptr; // Held (attached, not simulated): not in flight.
}

UPrimitiveComponent* UWB_PlayerDiveComponent::FindClosestBall(const FVector& From, float MaxDistance) const
{
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	UWorld* World = GetWorld();
	if (!World || !Profile || !Profile->BallClass)
	{
		return nullptr;
	}
	UPrimitiveComponent* Best = nullptr;
	float BestDistSq = FMath::Square(MaxDistance);
	for (TActorIterator<AActor> It(World, Profile->BallClass); It; ++It)
	{
		// Kicked or not: a held ball (not simulated) counts too, at its root.
		UPrimitiveComponent* Body = FindBallBody(*It);
		if (!Body)
		{
			Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
		}
		if (!Body)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Body->GetComponentLocation(), From);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Body;
		}
	}
	return Best;
}

bool UWB_PlayerDiveComponent::HasMoveInput() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && !Pawn->GetLastMovementInputVector().IsNearlyZero(KINDA_SMALL_NUMBER);
}

bool UWB_PlayerDiveComponent::TryStartDive()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Phase != EWB_DivePhase::None || bDefending || !PlayerState || !Character || !Character->GetCapsuleComponent() || !Character->GetMesh() || !GetActiveDiveProfile())
	{
		return false;
	}
	if (!PlayerState->CanEnterState(WBTags::Player_State_Diving))
	{
		return false;
	}
	PlanDive();
	if (!PlayerState->TryEnterState(WBTags::Player_State_Diving))
	{
		return false;
	}

	// Face the ball when one is in range (the body then tilts toward the jump direction: a side dive lies on its side, like a goalkeeper).
	FVector ToBall = FVector::ZeroVector;
	if (const UPrimitiveComponent* Ball = TargetBall.Get())
	{
		ToBall = (Ball->GetComponentLocation() - StartLocation).GetSafeNormal2D();
	}
	const FVector Facing = ToBall.IsNearlyZero() ? JumpDirection : ToBall;
	Character->SetActorRotation(FRotator(0.f, Facing.Rotation().Yaw, 0.f));

	// Lie on the side (user request 2026-11-17): the face ends perpendicular to the jump direction, on the side the player already faces.
	SideYaw = 0.f;
	if (GetActiveDiveProfile()->bLieOnSide)
	{
		const float FacingFromJump = FMath::FindDeltaAngleDegrees(JumpDirection.Rotation().Yaw, Facing.Rotation().Yaw);
		const float TargetFromJump = FacingFromJump >= 0.f ? 90.f : -90.f;
		SideYaw = FMath::FindDeltaAngleDegrees(FacingFromJump, TargetFromJump);
	}

	// Scripted flight: no gravity, no input (Diving blocks the movement inputs).
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}

	// Passes through the other players, like the Dash, without hitting them.
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	PawnResponseBefore = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	MeshBaseLocation = Mesh->GetRelativeLocation();
	MeshBaseRotation = Mesh->GetRelativeRotation();

	Elapsed = 0.f;
	PhaseElapsed = 0.f;
	bTouchedBall = false;
	bBlockedForward = false;
	CurrentTiltAlpha = 0.f;
	CurrentBodyOffset = 0.f;
	Phase = EWB_DivePhase::Takeoff;
	SetComponentTickEnabled(true);

	StartFlash();
	UE_LOG(LogWBPlayerDive, Verbose, TEXT("%s dives %.0f cm, body top %.0f cm at %.2f."), *GetNameSafe(Character), JumpDistance, PeakBodyHeight, PeakFraction);
	OnDiveStarted.Broadcast();
	return true;
}

void UWB_PlayerDiveComponent::PlanDive()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	UWorld* World = GetWorld();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	StartLocation = Character->GetActorLocation();
	StartGroundZ = StartLocation.Z - HalfHeight;

	// Stick direction (last move input, world space). The dive needs a move input; none (called from elsewhere): straight forward.
	FVector Stick = Character->GetLastMovementInputVector();
	Stick.Z = 0.f;
	Stick = Stick.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Stick.GetSafeNormal();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(Profile->StickConeHalfAngle));

	// Default: full length straight along the stick, in an arc (top a bit before the middle: fast rise, longer fall).
	JumpDirection = Stick;
	JumpDistance = Profile->MaxDistance;
	PeakBodyHeight = Profile->FreeDiveHeight;
	PeakFraction = Profile->FreeDivePeakFraction;

	UPrimitiveComponent* Ball = FindClosestBall(StartLocation, Profile->MaxBallDistance);
	TargetBall = Ball;
	bool bReachable = false;
	bool bInCone = false;
	const float T = FMath::Max(Profile->AirDuration, 0.2f);

	FVector P0 = FVector::ZeroVector;
	FVector V0 = FVector::ZeroVector;
	float GravityZ = 0.f;
	float BallRadius = 0.f;
	if (Ball && World)
	{
		// Ball path over the jump (gravity, no bounce; it does not go below the floor). A ball at rest or held: a fixed point.
		P0 = Ball->GetComponentLocation();
		const bool bSimulating = Ball->IsSimulatingPhysics();
		V0 = bSimulating ? Ball->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		GravityZ = bSimulating && Ball->IsGravityEnabled() ? World->GetGravityZ() : 0.f;
		BallRadius = Ball->Bounds.SphereRadius;
		const FVector StartFlat(StartLocation.X, StartLocation.Y, 0.f);

		float BestScore = TNumericLimits<float>::Max();
		float BestError = TNumericLimits<float>::Max();
		const float MinA = FMath::Clamp(Profile->MinInterceptFraction, 0.05f, 1.f);
		const float MaxA = FMath::Clamp(Profile->MaxInterceptFraction, MinA, 1.f);
		const int32 PathSamples = FMath::Max(Profile->PredictionSamples, 2);
		for (int32 i = 0; i < PathSamples; ++i)
		{
			const float A = FMath::Lerp(MinA, MaxA, static_cast<float>(i) / (PathSamples - 1));
			const float Time = A * T;
			FVector B = P0 + V0 * Time + FVector(0.f, 0.f, 0.5f * GravityZ * Time * Time);
			B.Z = FMath::Max(B.Z, StartGroundZ + BallRadius);

			const FVector Flat(B.X - StartFlat.X, B.Y - StartFlat.Y, 0.f);
			const float FlatDist = Flat.Size();
			const FVector Dir = FlatDist > 1.f ? Flat / FlatDist : Stick;
			const float Dot = FVector::DotProduct(Dir, Stick);
			if (Dot < CosCone)
			{
				continue; // Outside the stick cone: the stick wins.
			}
			// The body moves at constant speed: at fraction A it is A x Distance away.
			const float Distance = FMath::Clamp(FlatDist / A, Profile->MinDistance, Profile->MaxDistance);
			const float FlatError = FMath::Abs(FlatDist - Distance * A);
			const float BallHeight = B.Z - StartGroundZ;
			const float Height = FMath::Clamp(BallHeight, Profile->MinHeight, Profile->MaxHeight);
			const float Error = FMath::Sqrt(FMath::Square(FlatError) + FMath::Square(BallHeight - Height));

			auto Take = [&]()
			{
				JumpDirection = Dir;
				JumpDistance = Distance;
				PeakBodyHeight = Height;
				PeakFraction = A;
			};
			if (Error <= Profile->PlanningReach)
			{
				// Reachable: the point closest to the stick direction, then the earliest.
				const float Score = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f))) + A * Profile->InterceptTimeWeight;
				if (!bReachable || Score < BestScore)
				{
					BestScore = Score;
					bReachable = true;
					Take();
				}
			}
			else if (!bReachable && Error < BestError)
			{
				// In the cone but out of reach (too far, too high): still toward the ball, as close as the jump gets.
				BestError = Error;
				bInCone = true;
				Take();
			}
		}
	}
	PeakFraction = FMath::Clamp(PeakFraction, 0.1f, 0.9f);

	// Ground under the touchdown point (steps, slopes). Nothing close: same height as the start.
	EndGroundZ = StartGroundZ;
	const FVector EndFlat = StartLocation + JumpDirection * JumpDistance;
	if (World)
	{
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WBDiveGround), false, GetOwner());
		if (Ball)
		{
			Params.AddIgnoredActor(Ball->GetOwner());
		}
		if (World->LineTraceSingleByChannel(GroundHit, FVector(EndFlat.X, EndFlat.Y, StartGroundZ + Profile->LandingTraceUp), FVector(EndFlat.X, EndFlat.Y, StartGroundZ - Profile->LandingTraceDown), ECC_Visibility, Params))
		{
			EndGroundZ = GroundHit.ImpactPoint.Z;
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawDive && World)
	{
		if (Ball)
		{
			const int32 DebugSamples = FMath::Max(Profile->PredictionSamples, 2);
			for (int32 i = 0; i <= DebugSamples; ++i)
			{
				const float Time = T * i / DebugSamples;
				FVector B = P0 + V0 * Time + FVector(0.f, 0.f, 0.5f * GravityZ * Time * Time);
				B.Z = FMath::Max(B.Z, StartGroundZ + BallRadius);
				DrawDebugPoint(World, B, 8.f, FColor::Orange, false, 2.f);
			}
		}
		const FVector Peak = StartLocation + JumpDirection * (JumpDistance * PeakFraction);
		const FColor TargetColor = bReachable ? FColor::Green : (bInCone ? FColor::Red : FColor::White);
		DrawDebugSphere(World, FVector(Peak.X, Peak.Y, StartGroundZ + PeakBodyHeight), 20.f, 8, TargetColor, false, 2.f);
		DrawDebugLine(World, StartLocation, FVector(EndFlat.X, EndFlat.Y, EndGroundZ + HalfHeight), FColor::Cyan, false, 2.f, 0, 2.f);
	}
#endif
}

float UWB_PlayerDiveComponent::BodyHeightAt(float A) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float Standing = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const float Lying = GetActiveDiveProfile()->LyingHeight;
	// Ballistic arc (parabola): the rise starts at full vertical speed and slows to zero at the top, the fall then speeds up to the floor.
	if (A <= PeakFraction)
	{
		const float S = A / FMath::Max(PeakFraction, KINDA_SMALL_NUMBER);
		return FMath::Lerp(Standing, PeakBodyHeight, 1.f - FMath::Square(1.f - S));
	}
	const float S = (A - PeakFraction) / FMath::Max(1.f - PeakFraction, KINDA_SMALL_NUMBER);
	return FMath::Lerp(PeakBodyHeight, Lying, FMath::Square(S));
}

void UWB_PlayerDiveComponent::ApplyMeshPose(float TiltAlpha, float BodyOffset)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetMesh())
	{
		return;
	}
	CurrentTiltAlpha = TiltAlpha;
	CurrentBodyOffset = BodyOffset;

	// Tilt around the capsule center (hips), the head going toward the jump direction. Rotating Up around (Up x Dir) brings it toward Dir.
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, JumpDirection).GetSafeNormal();
	const FQuat WorldTilt = Axis.IsNearlyZero() ? FQuat::Identity : FQuat(Axis, FMath::DegreesToRadians(GetActiveDiveProfile()->BodyTilt * TiltAlpha));
	const FQuat CapsuleQuat = Character->GetCapsuleComponent()->GetComponentQuat();
	const FQuat LocalTilt = CapsuleQuat.Inverse() * WorldTilt * CapsuleQuat;

	// Side turn around the capsule axis first (bLieOnSide), so the tilt then lays the body on its side.
	const FQuat LocalSideYaw(FVector::UpVector, FMath::DegreesToRadians(SideYaw * TiltAlpha));

	const FVector Location = LocalTilt.RotateVector(LocalSideYaw.RotateVector(MeshBaseLocation)) + CapsuleQuat.UnrotateVector(FVector(0.f, 0.f, BodyOffset));
	const FQuat Rotation = LocalTilt * LocalSideYaw * MeshBaseRotation.Quaternion();
	Character->GetMesh()->SetRelativeLocationAndRotation(Location, Rotation);
}

void UWB_PlayerDiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	if (!Profile)
	{
		// Profile removed during play: stop right there.
		EndDefense(true);
		EndDive(true);
		SetComponentTickEnabled(false);
		return;
	}
	if (bDefending)
	{
		TickDefense(DeltaTime);
		return;
	}

	switch (Phase)
	{
	case EWB_DivePhase::Takeoff:
	case EWB_DivePhase::Air:
		TickAir(DeltaTime);
		break;

	case EWB_DivePhase::Land:
		PhaseElapsed += DeltaTime;
		if (PhaseElapsed >= LandDuration)
		{
			Phase = EWB_DivePhase::GetUp;
			PhaseElapsed = 0.f;
		}
		break;

	case EWB_DivePhase::GetUp:
	{
		// The body straightens up and rises back to the capsule while the get-up plays.
		PhaseElapsed += DeltaTime;
		const float S = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(PhaseElapsed / FMath::Max(GetUpDuration, KINDA_SMALL_NUMBER), 0.f, 1.f));
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		const float LyingOffset = Character ? Profile->LyingHeight - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		ApplyMeshPose(1.f - S, LyingOffset * (1.f - S));
		if (PhaseElapsed >= GetUpDuration)
		{
			EndDive(true);
		}
		break;
	}

	default:
		SetComponentTickEnabled(false);
		break;
	}
}

void UWB_PlayerDiveComponent::TickAir(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	if (!Character)
	{
		EndDive(false);
		return;
	}
	const float T = FMath::Max(Profile->AirDuration, 0.2f);
	Elapsed += DeltaTime;
	const float A = FMath::Clamp(Elapsed / T, 0.f, 1.f);
	Phase = Elapsed < FMath::Min(AnimLengthOr(Profile->JumpAnim, Profile->TakeoffFallbackDuration), T) ? EWB_DivePhase::Takeoff : EWB_DivePhase::Air;

	// Capsule: constant flat speed; it only rises when the body goes above standing height (a low dive keeps the capsule on the floor and lowers the body).
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float GroundZ = FMath::Lerp(StartGroundZ, EndGroundZ, A);
	const float BodyHeight = BodyHeightAt(A);
	const float CapsuleHeight = FMath::Max(BodyHeight, HalfHeight) + 1.f;
	const FVector PlannedFlat = StartLocation + JumpDirection * (JumpDistance * A);
	const FVector Flat = bBlockedForward ? BlockedFlatLocation : PlannedFlat;
	const FVector Target(Flat.X, Flat.Y, GroundZ + CapsuleHeight);

	FHitResult Hit;
	Character->SetActorLocation(Target, true, &Hit);
	if (Hit.bBlockingHit && !bBlockedForward)
	{
		// Wall or heavy object: the dive stops going forward there and only comes down.
		bBlockedForward = true;
		BlockedFlatLocation = Character->GetActorLocation();
		Character->SetActorLocation(FVector(BlockedFlatLocation.X, BlockedFlatLocation.Y, Target.Z), true);
	}

	const float Tilt = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Elapsed / FMath::Max(Profile->BodyTiltInTime, 0.01f), 0.f, 1.f));
	ApplyMeshPose(Tilt, BodyHeight + 1.f - CapsuleHeight);

	if (!bTouchedBall)
	{
		TestBallTouch();
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawDive)
	{
		const FVector Pivot = Character->GetActorLocation() + FVector(0.f, 0.f, CurrentBodyOffset);
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, JumpDirection).GetSafeNormal();
		const FVector BodyUp = FQuat(Axis, FMath::DegreesToRadians(Profile->BodyTilt * CurrentTiltAlpha)).RotateVector(FVector::UpVector);
		DrawDebugLine(GetWorld(), Pivot - BodyUp * HalfHeight, Pivot + BodyUp * (HalfHeight + Profile->ArmReach), bTouchedBall ? FColor::Green : FColor::Yellow, false, 0.f, 0, Profile->BodyRadius * 0.5f);
	}
#endif

	if (A >= 1.f)
	{
		Touchdown();
	}
}

void UWB_PlayerDiveComponent::TestBallTouch()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UPrimitiveComponent* Ball = TargetBall.Get();
	if (!Character || !Ball || !Ball->IsSimulatingPhysics())
	{
		return;
	}
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// The stretched body: from the feet to the hands, along the tilted body axis, around the hips.
	const FVector Pivot = Character->GetActorLocation() + FVector(0.f, 0.f, CurrentBodyOffset);
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, JumpDirection).GetSafeNormal();
	const FVector BodyUp = Axis.IsNearlyZero() ? FVector::UpVector : FQuat(Axis, FMath::DegreesToRadians(Profile->BodyTilt * CurrentTiltAlpha)).RotateVector(FVector::UpVector);
	const FVector Feet = Pivot - BodyUp * HalfHeight;
	const FVector Hands = Pivot + BodyUp * (HalfHeight + Profile->ArmReach);

	const FVector BallCenter = Ball->GetComponentLocation();
	const FVector Closest = FMath::ClosestPointOnSegment(BallCenter, Feet, Hands);
	const float BallRadius = Ball->Bounds.SphereRadius;
	if (FVector::DistSquared(BallCenter, Closest) > FMath::Square(Profile->BodyRadius + BallRadius))
	{
		return;
	}

	// The body moves at the jump's flat speed: a ball at rest is knocked away, a ball flying in bounces off.
	const FVector BodyVelocity = bBlockedForward ? FVector::ZeroVector : JumpDirection * (JumpDistance / FMath::Max(Profile->AirDuration, 0.2f));
	if (!BounceBall(Ball, Closest, BodyVelocity))
	{
		return;
	}
	bTouchedBall = true;
	UE_LOG(LogWBPlayerDive, Verbose, TEXT("%s touched %s."), *GetNameSafe(Character), *GetNameSafe(Ball->GetOwner()));
	OnDiveBallTouched.Broadcast(Ball->GetOwner());
}

bool UWB_PlayerDiveComponent::BounceBall(UPrimitiveComponent* Ball, const FVector& Contact, const FVector& BodyVelocity) const
{
	// Bounce away from the body, like on a wall moving at BodyVelocity: the speed toward the body (relative to it) is reversed, scaled by the restitution.
	const FVector Relative = Ball->GetPhysicsLinearVelocity() - BodyVelocity;
	FVector Normal = (Ball->GetComponentLocation() - Contact).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = -Relative.GetSafeNormal();
	}
	const float IntoBody = FVector::DotProduct(Relative, Normal);
	if (IntoBody >= 0.f)
	{
		return false; // Already leaving the body (or at rest against it).
	}
	const float Restitution = GetActiveDiveProfile()->BallBounceRestitution;
	const FVector NewVelocity = BodyVelocity + (Relative - IntoBody * Normal) * Restitution - IntoBody * Restitution * Normal;
	Ball->SetPhysicsLinearVelocity(NewVelocity);
	Ball->WakeAllRigidBodies();
	return true;
}

void UWB_PlayerDiveComponent::Touchdown()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	if (Character)
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			// Back to the normal movement: it finds the floor and lands (the inputs stay blocked by Diving).
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Falling);
		}
		ApplyMeshPose(1.f, Profile->LyingHeight - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	Phase = EWB_DivePhase::Land;
	PhaseElapsed = 0.f;
	// The landing and the get-up play at RecoveryPlayRate (ABP_SoccerPlayer uses the same rate): their phases are that much shorter.
	const float RecoveryRate = FMath::Max(Profile->RecoveryPlayRate, 0.1f);
	LandDuration = AnimLengthOr(Profile->LandAnim, Profile->LandFallbackDuration) / RecoveryRate + (bTouchedBall ? 0.f : Profile->MissStunDuration);
	GetUpDuration = FMath::Max(AnimLengthOr(Profile->GetUpAnim, Profile->GetUpFallbackDuration) / RecoveryRate, 0.01f);
	OnDiveLanded.Broadcast(bTouchedBall);
}

void UWB_PlayerDiveComponent::EndDive(bool bReturnToFree)
{
	if (Phase == EWB_DivePhase::None)
	{
		return;
	}
	Phase = EWB_DivePhase::None;
	SetComponentTickEnabled(false);

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->SetRelativeLocationAndRotation(MeshBaseLocation, MeshBaseRotation);
		}
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, PawnResponseBefore);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (Movement->MovementMode == MOVE_Flying)
			{
				Movement->SetMovementMode(MOVE_Falling);
			}
		}
	}
	CurrentTiltAlpha = 0.f;
	CurrentBodyOffset = 0.f;

	if (bReturnToFree && PlayerState && PlayerState->IsInState(WBTags::Player_State_Diving))
	{
		TGuardValue<bool> Guard(bChangingStateItself, true);
		PlayerState->ChangePlayerState(WBTags::Player_State_Free);
	}
	OnDiveFinished.Broadcast(bTouchedBall);
}

bool UWB_PlayerDiveComponent::TryStartDefense()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	if (Phase != EWB_DivePhase::None || bDefending || !PlayerState || !Character || !Profile)
	{
		return false;
	}
	if (!PlayerState->TryEnterState(WBTags::Player_State_Defending))
	{
		return false;
	}
	bDefending = true;
	// Single aim at the start (user rule 2026-11-17, 14 h 53): a ball within DefenseBallTrackRadius gives the yaw to reach, once; farther, the current
	// facing. The target is never computed again: once reached, the facing stays locked even if the ball moves.
	DefenseYaw = Character->GetActorRotation().Yaw;
	if (const UPrimitiveComponent* Ball = FindClosestBall(Character->GetActorLocation(), Profile->DefenseBallTrackRadius))
	{
		const FVector ToBall = (Ball->GetComponentLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		if (!ToBall.IsNearlyZero())
		{
			DefenseYaw = ToBall.Rotation().Yaw;
		}
	}
	bDefenseTurning = !FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, DefenseYaw), 0.5f);

	// The component holds the facing itself; the moves come in the player's axes (AddDefenseMoveInput).
	bUseControllerRotationYawBefore = Character->bUseControllerRotationYaw;
	Character->bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		bOrientRotationToMovementBefore = Movement->bOrientRotationToMovement;
		bUseControllerDesiredRotationBefore = Movement->bUseControllerDesiredRotation;
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
	}
	SetComponentTickEnabled(true);
	return true;
}

void UWB_PlayerDiveComponent::StopDefense()
{
	if (bDefending)
	{
		EndDefense(true);
	}
}

void UWB_PlayerDiveComponent::EndDefense(bool bReturnToFree)
{
	if (!bDefending)
	{
		return;
	}
	bDefending = false;
	bDefenseTurning = false;
	SetComponentTickEnabled(false);

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->bUseControllerRotationYaw = bUseControllerRotationYawBefore;
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bOrientRotationToMovementBefore;
			Movement->bUseControllerDesiredRotation = bUseControllerDesiredRotationBefore;
		}
	}
	if (bReturnToFree && PlayerState && PlayerState->IsInState(WBTags::Player_State_Defending))
	{
		TGuardValue<bool> Guard(bChangingStateItself, true);
		PlayerState->ChangePlayerState(WBTags::Player_State_Free);
	}
}

void UWB_PlayerDiveComponent::AddDefenseMoveInput(float RightAxis, float ForwardAxis)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	if (!bDefending || !Pawn || !Profile)
	{
		return;
	}
	// Camera-relative stick, exactly like the normal Move (user rule 2026-11-17): the yaw of the camera, flattened. The body facing is not used to
	// read the input, so W / A / S / D always follow the screen in straight lines.
	const APlayerCameraManager* Camera = nullptr;
	if (const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
	{
		Camera = PlayerController->PlayerCameraManager;
	}
	if (!Camera)
	{
		Camera = UGameplayStatics::GetPlayerCameraManager(this, 0); // Bots and the shared camera.
	}
	const FRotationMatrix CameraYaw(FRotator(0.f, Camera ? Camera->GetCameraRotation().Yaw : 0.f, 0.f));
	const FVector Desired = CameraYaw.GetUnitAxis(EAxis::Y) * RightAxis + CameraYaw.GetUnitAxis(EAxis::X) * ForwardAxis;

	// Speed split along the locked facing: forward / back and side steps each have their own share of the walk speed.
	const FRotationMatrix BodyYaw(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f));
	const FVector BodyForward = BodyYaw.GetUnitAxis(EAxis::X);
	const FVector BodyRight = BodyYaw.GetUnitAxis(EAxis::Y);
	const float Speed = PlayerState ? PlayerState->GetMoveSpeedMultiplier() : 1.f;
	Pawn->AddMovementInput(BodyForward, FVector::DotProduct(Desired, BodyForward) * Profile->DefenseForwardSpeedMultiplier * Speed);
	Pawn->AddMovementInput(BodyRight, FVector::DotProduct(Desired, BodyRight) * Profile->DefenseSideSpeedMultiplier * Speed);
}

float UWB_PlayerDiveComponent::GetDefenseMoveAngle() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.f;
	}
	const FVector Velocity = Owner->GetVelocity().GetSafeNormal2D();
	if (Velocity.IsNearlyZero())
	{
		return 0.f;
	}
	const FRotationMatrix BodyYaw(FRotator(0.f, Owner->GetActorRotation().Yaw, 0.f));
	return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Velocity, BodyYaw.GetUnitAxis(EAxis::Y)), FVector::DotProduct(Velocity, BodyYaw.GetUnitAxis(EAxis::X))));
}

void UWB_PlayerDiveComponent::TickDefense(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		EndDefense(false);
		return;
	}
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();

	// Single turn toward the yaw chosen at the start, then the facing stays locked there (no ball tracking).
	float Yaw = DefenseYaw;
	if (bDefenseTurning)
	{
		Yaw = FMath::FixedTurn(Character->GetActorRotation().Yaw, DefenseYaw, Profile->DefenseTurnSpeed * DeltaTime);
		bDefenseTurning = !FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Yaw, DefenseYaw), 0.5f);
		if (!bDefenseTurning)
		{
			Yaw = DefenseYaw;
		}
	}
	Character->SetActorRotation(FRotator(0.f, Yaw, 0.f));

	UPrimitiveComponent* Ball = FindClosestBall(Character->GetActorLocation(), Profile->MaxBallDistance);

	// Catch: the spread body is a box (width of the X, thickness, capsule height plus the raised hands) facing the ball.
	if (Ball && Ball->IsSimulatingPhysics())
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Center = Character->GetActorLocation() + FVector(0.f, 0.f, Profile->DefenseBlockExtraHeight * 0.5f);
		const FVector Extent(Profile->DefenseBlockHalfThickness, Profile->DefenseBlockHalfWidth, HalfHeight + Profile->DefenseBlockExtraHeight * 0.5f);
		const FQuat Facing = FRotator(0.f, Character->GetActorRotation().Yaw, 0.f).Quaternion();
		const FVector BallCenter = Ball->GetComponentLocation();
		const FVector Local = Facing.UnrotateVector(BallCenter - Center);
		const FVector ClosestLocal(FMath::Clamp(Local.X, -Extent.X, Extent.X), FMath::Clamp(Local.Y, -Extent.Y, Extent.Y), FMath::Clamp(Local.Z, -Extent.Z, Extent.Z));
		const FVector Closest = Center + Facing.RotateVector(ClosestLocal);
		if (FVector::DistSquared(BallCenter, Closest) <= FMath::Square(Ball->Bounds.SphereRadius))
		{
			// Interception (user rule 2026-11-17): the defender catches the ball, kicked or not, unless another player holds it.
			AActor* BallActor = Ball->GetOwner();
			if (!UWB_BallPossessionSubsystem::IsBallHeld(this, BallActor))
			{
				UE_LOG(LogWBPlayerDive, Verbose, TEXT("%s caught %s in defense."), *GetNameSafe(Character), *GetNameSafe(BallActor));
				UWB_BallPossessionSubsystem::ClearBallKick(this, BallActor);
				EndDefense(true);
				OnDefenseBallCaught.Broadcast(BallActor);
				return;
			}
		}

#if ENABLE_DRAW_DEBUG
		if (bDebugDrawDive)
		{
			DrawDebugBox(GetWorld(), Center, Extent, Facing, FColor::Yellow, false, 0.f, 0, 1.f);
		}
#endif
	}
}

void UWB_PlayerDiveComponent::HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState)
{
	if (bChangingStateItself)
	{
		return;
	}
	// Another state cut the dive or the stance short (hit, goal, Disabled): stop right there, the new state takes over.
	if (OldState == WBTags::Player_State_Diving && Phase != EWB_DivePhase::None)
	{
		EndDive(false);
	}
	if (OldState == WBTags::Player_State_Defending && bDefending)
	{
		EndDefense(false);
	}
}

void UWB_PlayerDiveComponent::StartFlash()
{
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Profile || !Profile->FlashMaterial || !Character || !Character->GetMesh() || !World)
	{
		return;
	}
	// Another overlay is shown (post-Down grace blink): leave it alone.
	if (Character->GetMesh()->GetOverlayMaterial())
	{
		return;
	}
	FlashShown = Profile->FlashMaterial;
	Character->GetMesh()->SetOverlayMaterial(FlashShown);
	World->GetTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateUObject(this, &UWB_PlayerDiveComponent::EndFlash), Profile->FlashDuration, false);
}

void UWB_PlayerDiveComponent::EndFlash()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetMesh() && FlashShown && Character->GetMesh()->GetOverlayMaterial() == FlashShown)
	{
		Character->GetMesh()->SetOverlayMaterial(nullptr);
	}
	FlashShown = nullptr;
}
