#include "Grab/WB_HeldObjectMotionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Grab/WB_GrabLibrary.h"
#include "Grab/WB_HeldObjectMotionProfile.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBHeldObject, Log, All);

namespace
{
	/** Largest step of the weight spring, in seconds: keeps it stable at low frame rates. */
	constexpr float MaxSpringStep = 1.f / 120.f;
}

UWB_HeldObjectMotionComponent::UWB_HeldObjectMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The Physics Handle reads its target during the physics step: move it just before.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UWB_HeldObjectMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	Character = Cast<ACharacter>(Owner);
	PhysicsHandle = Owner ? Owner->FindComponentByClass<UPhysicsHandleComponent>() : nullptr;
	PlayerState = Owner ? Owner->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
}

void UWB_HeldObjectMotionComponent::StartHolding(UPrimitiveComponent* Held, bool bAtPoint, bool bIsBall)
{
	if (!Profile || !Character || !PhysicsHandle || !Held)
	{
		UE_LOG(LogWBHeldObject, Warning, TEXT("%s: StartHolding needs a Profile, a Character owner with a Physics Handle, and a held component. Held object not moved."),
			*GetPathNameSafe(this));
		return;
	}

	HeldComponent = Held;
	bHeldAtPoint = bAtPoint;
	bHeldIsBall = bIsBall;
	WeightOffset = FVector::ZeroVector;
	WeightVelocity = FVector::ZeroVector;
	Tilt = FRotator::ZeroRotator;
	SetComponentTickEnabled(true);
}

void UWB_HeldObjectMotionComponent::StopHolding()
{
	HeldComponent.Reset();
	SetComponentTickEnabled(false);
}

FVector UWB_HeldObjectMotionComponent::ComputeCarryPoint(const UPrimitiveComponent* Held) const
{
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector CapsuleLocation = Capsule->GetComponentLocation();
	const FVector Forward = Capsule->GetForwardVector().GetSafeNormal2D();
	const float FeetZ = CapsuleLocation.Z - Capsule->GetScaledCapsuleHalfHeight();
	const float Height = bHeldIsBall ? Profile->BallCarryHeight : Profile->ObjectCarryHeight;

	FVector Carry(CapsuleLocation.X, CapsuleLocation.Y, FeetZ + Height);
	Carry += Forward * Profile->CarryDistance;

	// Throw windup of a large object: the target travels on an arc over the head.
	if (PlayerState && PlayerState->IsInState(WBTags::Player_State_Throwing) && UWB_GrabLibrary::IsLargeHeldObject(Held, Profile->ThrowArcMinBoundsRadius))
	{
		Carry = UWB_GrabLibrary::GetOverheadThrowTarget(Carry, Capsule, Held, PlayerState->ActionCharge,
			Profile->ThrowArcHeight, Profile->ThrowOverheadHeight, Profile->ThrowOverheadForward, Profile->ThrowClearanceMargin);
	}
	return Carry;
}

void UWB_HeldObjectMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UPrimitiveComponent* Held = HeldComponent.Get();
	if (!Held || !Profile || !Character || !PhysicsHandle || (PlayerState && !PlayerState->IsHoldingAnything()))
	{
		StopHolding();
		return;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	// Grabbed at a point: only the location, straight in front of the capsule.
	if (bHeldAtPoint)
	{
		LastTargetLocation = Capsule->GetComponentLocation() + Capsule->GetForwardVector() * Profile->CarryDistance;
		PhysicsHandle->SetTargetLocation(LastTargetLocation);
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Velocity = Movement ? Movement->Velocity : Character->GetVelocity();
	const float MaxWalkSpeed = Movement ? FMath::Max(Movement->MaxWalkSpeed, 1.f) : 1.f;
	const float SpeedAlpha = FMath::Clamp(Velocity.Size() / MaxWalkSpeed, 0.f, 1.f);

	// Sways: phases advance in degrees per second, faster while moving.
	PositionSwayPhase = FMath::Fmod(PositionSwayPhase + FMath::Lerp(Profile->PositionSwaySpeedIdle, Profile->PositionSwaySpeedMoving, SpeedAlpha) * DeltaTime, 360.f);
	RotationSwayPhase = FMath::Fmod(RotationSwayPhase + FMath::Lerp(Profile->RotationSwaySpeedIdle, Profile->RotationSwaySpeedMoving, SpeedAlpha) * DeltaTime, 360.f);

	const float PosSin = FMath::Sin(FMath::DegreesToRadians(PositionSwayPhase));
	const float PosCos = FMath::Cos(FMath::DegreesToRadians(PositionSwayPhase));
	const FVector PositionSway(0.f, PosCos * Profile->PositionSwayRadius, PosSin * Profile->PositionSwayRadius);

	const float RotSin = FMath::Sin(FMath::DegreesToRadians(RotationSwayPhase));
	const float RotCos = FMath::Cos(FMath::DegreesToRadians(RotationSwayPhase));
	const FRotator RotationSway(RotSin * Profile->RotationSwayAmplitude.Pitch, RotCos * Profile->RotationSwayAmplitude.Yaw, RotSin * Profile->RotationSwayAmplitude.Roll);

	// Velocity lag: the side and vertical parts of the velocity, in the player's own control frame.
	const FRotator ControlRotation = Character->GetController() ? Character->GetControlRotation() : Character->GetActorRotation();
	FVector LocalVelocity = ControlRotation.UnrotateVector(Velocity);
	LocalVelocity.X = 0.f;
	const float Mass = Held->IsSimulatingPhysics() ? Held->GetMass() : 0.f;
	const float MassFactor = FMath::GetMappedRangeValueClamped(FVector2f(Profile->LagLightMass, FMath::Max(Profile->LagHeavyMass, Profile->LagLightMass + 1.f)),
		FVector2f(Profile->LagLightMassFactor, Profile->LagHeavyMassFactor), Mass);
	const FVector VelocityLag = ControlRotation.RotateVector(LocalVelocity) * Profile->VelocityLagMultiplier * MassFactor;

	// Weight sag: a damped spring follows an offset toward the player's right and up, proportional to the mass.
	const FVector WeightTarget = (Capsule->GetRightVector() + Capsule->GetUpVector()) * (Mass * Profile->WeightOffsetPerKg);
	const float Omega = 2.f * PI * Profile->WeightSpringFrequency;
	float Remaining = DeltaTime;
	while (Remaining > UE_KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, MaxSpringStep);
		WeightVelocity += (Omega * Omega * (WeightTarget - WeightOffset) - 2.f * Profile->WeightSpringDamping * Omega * WeightVelocity) * Step;
		WeightOffset += WeightVelocity * Step;
		Remaining -= Step;
	}

	// Velocity tilt (off by default).
	FRotator TiltTarget = FRotator::ZeroRotator;
	if (Profile->bEnableVelocityTilt)
	{
		const FVector YawVelocity = FRotator(0.f, ControlRotation.Yaw, 0.f).UnrotateVector(Velocity);
		TiltTarget.Roll = FMath::Clamp(YawVelocity.Y / MaxWalkSpeed, -1.f, 1.f) * Profile->MaxTiltAngle;
		TiltTarget.Pitch = FMath::Clamp(YawVelocity.X / MaxWalkSpeed, -1.f, 1.f) * Profile->MaxTiltAngle;
	}
	const float TiltAlpha = 1.f - FMath::Exp(-Profile->TiltFollowSpeed * DeltaTime);
	Tilt = FMath::Lerp(Tilt, TiltTarget, TiltAlpha);

	LastTargetLocation = ComputeCarryPoint(Held) + PositionSway - VelocityLag + WeightOffset;

	// Same order as the old Blueprint (Combine Rotators): tilt, then sway, then the capsule yaw.
	const FQuat YawQuat = FRotator(0.f, Capsule->GetComponentRotation().Yaw, 0.f).Quaternion();
	const FQuat TargetQuat = YawQuat * (RotationSway.Quaternion() * Tilt.Quaternion());
	PhysicsHandle->SetTargetLocationAndRotation(LastTargetLocation, TargetQuat.Rotator());
}
