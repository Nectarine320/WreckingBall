#include "Player/Dive/WB_PlayerDiveComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Feedback/WB_FeedbackLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Player/Dive/WB_PlayerDiveProfile.h"
#include "Player/Hit/WB_PlayerHitComponent.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerDive, Log, All);

namespace
{
	/** Number of points of the predicted ball path tested when planning the dive. */
	constexpr int32 PathSamples = 24;

	float AnimLengthOr(const UAnimSequenceBase* Anim, float Fallback)
	{
		// Real playing time: the asset's RateScale speeds it up or slows it down.
		return Anim ? Anim->GetPlayLength() / FMath::Max(FMath::Abs(Anim->RateScale), 0.01f) : Fallback;
	}
}

UWB_PlayerDiveComponent::UWB_PlayerDiveComponent()
{
	// Ticks only during a dive.
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
	return DiveProfile ? DiveProfile.Get() : GetDefault<UWB_PlayerDiveProfile>();
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

bool UWB_PlayerDiveComponent::IsEnemyBall(AActor* Ball) const
{
	if (!GetActiveDiveProfile()->bOnlyEnemyBall)
	{
		return true;
	}
	const APawn* Responsible = UWB_FeedbackLibrary::GetResponsiblePawn(Ball);
	if (!Responsible)
	{
		return true; // Nobody sent it (or it fell asleep since): counts as enemy.
	}
	if (Responsible == GetOwner())
	{
		return false;
	}
	const UWB_PlayerHitComponent* MyHit = GetOwner()->FindComponentByClass<UWB_PlayerHitComponent>();
	const UWB_PlayerHitComponent* OtherHit = Responsible->FindComponentByClass<UWB_PlayerHitComponent>();
	if (MyHit && OtherHit && MyHit->TeamIndex >= 0 && MyHit->TeamIndex == OtherHit->TeamIndex)
	{
		return false; // Teammate's ball.
	}
	return true;
}

bool UWB_PlayerDiveComponent::TryStartDive()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Phase != EWB_DivePhase::None || !PlayerState || !Character || !Character->GetCapsuleComponent() || !Character->GetMesh())
	{
		return false;
	}
	// Cheap checks first (state table, empty hands), then the ball search.
	if (!PlayerState->CanEnterState(WBTags::Player_State_Diving) || !PlanDive())
	{
		return false;
	}
	if (!PlayerState->TryEnterState(WBTags::Player_State_Diving))
	{
		return false;
	}

	// Face the ball (the body then tilts toward the jump direction: a side dive lies on its side, like a goalkeeper).
	FVector ToBall = FVector::ZeroVector;
	if (const UPrimitiveComponent* Ball = TargetBall.Get())
	{
		ToBall = (Ball->GetComponentLocation() - StartLocation).GetSafeNormal2D();
	}
	const FVector Facing = ToBall.IsNearlyZero() ? JumpDirection : ToBall;
	Character->SetActorRotation(FRotator(0.f, Facing.Rotation().Yaw, 0.f));

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

bool UWB_PlayerDiveComponent::PlanDive()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	UWorld* World = GetWorld();
	if (!Character || !World || !Profile->BallClass)
	{
		return false;
	}
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	StartLocation = Character->GetActorLocation();
	StartGroundZ = StartLocation.Z - HalfHeight;

	// Closest ball in flight, sent by the other team.
	UPrimitiveComponent* BestBall = nullptr;
	float BestDistSq = FMath::Square(Profile->MaxBallDistance);
	for (TActorIterator<AActor> It(World, Profile->BallClass); It; ++It)
	{
		UPrimitiveComponent* Body = FindBallBody(*It);
		if (!Body || Body->GetPhysicsLinearVelocity().Size() < Profile->MinBallSpeed || !IsEnemyBall(*It))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Body->GetComponentLocation(), StartLocation);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestBall = Body;
		}
	}
	if (!BestBall)
	{
		return false;
	}
	TargetBall = BestBall;

	// Stick direction (last move input, world space). Released: no cone.
	FVector Stick = Character->GetLastMovementInputVector();
	Stick.Z = 0.f;
	const bool bHasStick = Stick.SizeSquared() > FMath::Square(0.2f);
	Stick = Stick.GetSafeNormal();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(Profile->StickConeHalfAngle));

	// Ball path over the jump (gravity, no bounce; it does not go below the floor).
	const FVector P0 = BestBall->GetComponentLocation();
	const FVector V0 = BestBall->GetPhysicsLinearVelocity();
	const float GravityZ = BestBall->IsGravityEnabled() ? World->GetGravityZ() : 0.f;
	const float BallRadius = BestBall->Bounds.SphereRadius;
	const float T = FMath::Max(Profile->AirDuration, 0.2f);
	const FVector StartFlat(StartLocation.X, StartLocation.Y, 0.f);

	bool bFound = false;
	float BestScore = TNumericLimits<float>::Max();
	const float MinA = FMath::Clamp(Profile->MinInterceptFraction, 0.05f, 1.f);
	const float MaxA = FMath::Clamp(Profile->MaxInterceptFraction, MinA, 1.f);
	for (int32 i = 0; i < PathSamples; ++i)
	{
		const float A = FMath::Lerp(MinA, MaxA, PathSamples > 1 ? static_cast<float>(i) / (PathSamples - 1) : 0.f);
		const float Time = A * T;
		FVector B = P0 + V0 * Time + FVector(0.f, 0.f, 0.5f * GravityZ * Time * Time);
		B.Z = FMath::Max(B.Z, StartGroundZ + BallRadius);

		const FVector Flat(B.X - StartFlat.X, B.Y - StartFlat.Y, 0.f);
		const float FlatDist = Flat.Size();
		const FVector Dir = FlatDist > 1.f ? Flat / FlatDist : (bHasStick ? Stick : Character->GetActorForwardVector().GetSafeNormal2D());
		if (bHasStick && FVector::DotProduct(Dir, Stick) < CosCone)
		{
			continue; // Outside the stick cone.
		}
		// The body moves at constant speed: at fraction A it is A x Distance away.
		const float Distance = FMath::Clamp(FlatDist / A, Profile->MinDistance, Profile->MaxDistance);
		const float FlatError = FMath::Abs(FlatDist - Distance * A);
		const float BallHeight = B.Z - StartGroundZ;
		const float Height = FMath::Clamp(BallHeight, Profile->MinHeight, Profile->MaxHeight);
		const float Error = FMath::Sqrt(FMath::Square(FlatError) + FMath::Square(BallHeight - Height));
		if (Error > Profile->PlanningReach)
		{
			continue;
		}
		// With the stick: the point closest to the stick direction (then the earliest). Without: the earliest point (most urgent).
		const float Score = bHasStick ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Stick), -1.f, 1.f))) + A * 10.f : A;
		if (Score < BestScore)
		{
			BestScore = Score;
			bFound = true;
			JumpDirection = Dir;
			JumpDistance = Distance;
			PeakBodyHeight = Height;
			PeakFraction = A;
		}
	}

	if (!bFound)
	{
		// No reachable point: full length along the stick, or toward the ball, low.
		const FVector ToBall = FVector(P0.X - StartFlat.X, P0.Y - StartFlat.Y, 0.f).GetSafeNormal();
		JumpDirection = bHasStick ? Stick : (ToBall.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : ToBall);
		JumpDistance = Profile->MaxDistance;
		PeakBodyHeight = Profile->MinHeight;
		PeakFraction = 0.5f;
	}
	PeakFraction = FMath::Clamp(PeakFraction, 0.1f, 0.9f);

	// Ground under the touchdown point (steps, slopes). Nothing close: same height as the start.
	EndGroundZ = StartGroundZ;
	const FVector EndFlat = StartLocation + JumpDirection * JumpDistance;
	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBDiveGround), false, GetOwner());
	Params.AddIgnoredActor(BestBall->GetOwner());
	if (World->LineTraceSingleByChannel(GroundHit, FVector(EndFlat.X, EndFlat.Y, StartGroundZ + 100.f), FVector(EndFlat.X, EndFlat.Y, StartGroundZ - 200.f), ECC_Visibility, Params))
	{
		EndGroundZ = GroundHit.ImpactPoint.Z;
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawDive)
	{
		for (int32 i = 0; i <= PathSamples; ++i)
		{
			const float Time = T * i / PathSamples;
			FVector B = P0 + V0 * Time + FVector(0.f, 0.f, 0.5f * GravityZ * Time * Time);
			B.Z = FMath::Max(B.Z, StartGroundZ + BallRadius);
			DrawDebugPoint(World, B, 8.f, FColor::Orange, false, 2.f);
		}
		const FVector Peak = StartLocation + JumpDirection * (JumpDistance * PeakFraction);
		DrawDebugSphere(World, FVector(Peak.X, Peak.Y, StartGroundZ + PeakBodyHeight), 20.f, 8, bFound ? FColor::Green : FColor::Red, false, 2.f);
		DrawDebugLine(World, StartLocation, FVector(EndFlat.X, EndFlat.Y, EndGroundZ + HalfHeight), FColor::Cyan, false, 2.f, 0, 2.f);
	}
#endif
	return true;
}

float UWB_PlayerDiveComponent::BodyHeightAt(float A) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float Standing = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const float Lying = GetActiveDiveProfile()->LyingHeight;
	if (A <= PeakFraction)
	{
		// Rise fast, slow at the top.
		const float S = A / FMath::Max(PeakFraction, KINDA_SMALL_NUMBER);
		return FMath::Lerp(Standing, PeakBodyHeight, FMath::Sin(S * HALF_PI));
	}
	// Fall slowly from the top, fast at the end (gravity feel), down to the floor.
	const float S = (A - PeakFraction) / FMath::Max(1.f - PeakFraction, KINDA_SMALL_NUMBER);
	return FMath::Lerp(PeakBodyHeight, Lying, 1.f - FMath::Cos(S * HALF_PI));
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

	const FVector Location = LocalTilt.RotateVector(MeshBaseLocation) + CapsuleQuat.UnrotateVector(FVector(0.f, 0.f, BodyOffset));
	const FQuat Rotation = LocalTilt * MeshBaseRotation.Quaternion();
	Character->GetMesh()->SetRelativeLocationAndRotation(Location, Rotation);
}

void UWB_PlayerDiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
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
	Phase = Elapsed < FMath::Min(AnimLengthOr(Profile->JumpAnim, 0.2f), T) ? EWB_DivePhase::Takeoff : EWB_DivePhase::Air;

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

	// Bounce away from the body, like on a wall: the speed toward the body is reversed, scaled by the restitution.
	const FVector Velocity = Ball->GetPhysicsLinearVelocity();
	FVector Normal = (BallCenter - Closest).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = -Velocity.GetSafeNormal();
	}
	const float Restitution = Profile->BallBounceRestitution;
	const float IntoBody = FVector::DotProduct(Velocity, Normal);
	FVector NewVelocity = Velocity;
	if (IntoBody < 0.f)
	{
		NewVelocity = (Velocity - IntoBody * Normal) * Restitution - IntoBody * Restitution * Normal;
	}
	Ball->SetPhysicsLinearVelocity(NewVelocity);
	Ball->WakeAllRigidBodies();

	bTouchedBall = true;
	UE_LOG(LogWBPlayerDive, Verbose, TEXT("%s touched %s (%.0f -> %.0f cm/s)."), *GetNameSafe(Character), *GetNameSafe(Ball->GetOwner()), Velocity.Size(), NewVelocity.Size());
	OnDiveBallTouched.Broadcast(Ball->GetOwner());
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
	LandDuration = AnimLengthOr(Profile->LandAnim, Profile->LandFallbackDuration) + (bTouchedBall ? 0.f : Profile->MissStunDuration);
	GetUpDuration = FMath::Max(AnimLengthOr(Profile->GetUpAnim, Profile->GetUpFallbackDuration), 0.01f);
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

void UWB_PlayerDiveComponent::HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState)
{
	// Another state cut the dive short (hit, goal, Disabled): stop right there, the new state takes over.
	if (OldState == WBTags::Player_State_Diving && Phase != EWB_DivePhase::None && !bChangingStateItself)
	{
		EndDive(false);
	}
}

void UWB_PlayerDiveComponent::StartFlash()
{
	const UWB_PlayerDiveProfile* Profile = GetActiveDiveProfile();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Profile->FlashMaterial || !Character || !Character->GetMesh() || !World)
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
