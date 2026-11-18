#include "Player/Surface/WB_SurfaceMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/Surface/WB_SurfaceMovementProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBSurface, Log, All);

namespace
{
	/** Half length of the short trace on the floor component that reads its Physical Material. Technical value. */
	constexpr float FloorProbeHalfLength = 10.f;
}

UWB_SurfaceMovementComponent::UWB_SurfaceMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWB_SurfaceMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		BaseMaxAcceleration = Movement->MaxAcceleration;
		BaseBrakingDeceleration = Movement->BrakingDecelerationWalking;
		BaseGroundFriction = Movement->GroundFriction;
		bHasBaseValues = true;
	}
	if (!SurfaceTable)
	{
		// No silent fallback: without its table the floors change nothing.
		UE_LOG(LogWBSurface, Warning, TEXT("%s: no SurfaceTable on %s (DA_SurfaceMovementTable expected). The floors do not change the movement."), *GetNameSafe(GetOwner()), *GetName());
		SetComponentTickEnabled(false);
	}
}

float UWB_SurfaceMovementComponent::GetDashDistanceMultiplier() const
{
	const UWB_SurfaceMovementProfile* Profile = CurrentProfile.Get();
	return Profile ? Profile->DashDistanceMultiplier : 1.f;
}

TEnumAsByte<EPhysicalSurface> UWB_SurfaceMovementComponent::ReadFloorSurface(bool& bOutOnGround) const
{
	bOutOnGround = false;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->IsMovingOnGround() || !Movement->CurrentFloor.IsWalkableFloor())
	{
		return CurrentSurface;
	}
	bOutOnGround = true;
	const FHitResult& FloorHit = Movement->CurrentFloor.HitResult;
	UPrimitiveComponent* Floor = FloorHit.GetComponent();
	if (!Floor)
	{
		return SurfaceType_Default;
	}
	// The movement floor sweep does not return the Physical Material: a short trace on the floor component itself does.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBSurfaceProbe), true);
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	const FVector Point = FloorHit.ImpactPoint;
	if (Floor->LineTraceComponent(Hit, Point + FVector::UpVector * FloorProbeHalfLength, Point - FVector::UpVector * FloorProbeHalfLength, Params))
	{
		return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	}
	return SurfaceType_Default;
}

void UWB_SurfaceMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bOnGround = false;
	const TEnumAsByte<EPhysicalSurface> Surface = ReadFloorSurface(bOnGround);
	// In the air the last floor stays applied; on the ground, only a change of surface applies a new profile.
	if (bOnGround && (!bSurfaceKnown || Surface != CurrentSurface))
	{
		ApplySurface(Surface);
	}
}

void UWB_SurfaceMovementComponent::ApplySurface(TEnumAsByte<EPhysicalSurface> Surface)
{
	CurrentSurface = Surface;
	bSurfaceKnown = true;
	const UWB_SurfaceMovementProfile* Profile = SurfaceTable ? SurfaceTable->FindProfile(Surface) : nullptr;
	CurrentProfile = Profile;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		if (bHasBaseValues)
		{
			// Always from the base values: walking from ice to carpet never stacks the two profiles.
			Movement->MaxAcceleration = BaseMaxAcceleration * (Profile ? Profile->MaxAccelerationMultiplier : 1.f);
			Movement->BrakingDecelerationWalking = BaseBrakingDeceleration * (Profile ? Profile->BrakingDecelerationMultiplier : 1.f);
			Movement->GroundFriction = BaseGroundFriction * (Profile ? Profile->GroundFrictionMultiplier : 1.f);
		}
	}
	if (UWB_PlayerStateComponent* State = GetOwner() ? GetOwner()->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr)
	{
		State->SetSurfaceSpeedMultiplier(Profile ? Profile->MaxWalkSpeedMultiplier : 1.f);
	}
	OnSurfaceChanged.Broadcast(Surface);
}
