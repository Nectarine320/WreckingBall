#include "Impact/Destructible/Door/WB_HingedDoorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Impact/Destructible/Door/WB_HingedDoorProfile.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBHingedDoor, Log, All);

UWB_HingedDoorComponent::UWB_HingedDoorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_HingedDoorComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeDoor();
}

UPrimitiveComponent* UWB_HingedDoorComponent::GetBody() const
{
	return Cast<UPrimitiveComponent>(DoorBody.GetComponent(GetOwner()));
}

UPhysicsConstraintComponent* UWB_HingedDoorComponent::GetConstraint() const
{
	return Cast<UPhysicsConstraintComponent>(HingeConstraint.GetComponent(GetOwner()));
}

USceneComponent* UWB_HingedDoorComponent::FindHingePivot() const
{
	const AActor* Owner = GetOwner();
	const AActor* Wall = Owner ? Owner->GetAttachParentActor() : nullptr;
	if (!Wall)
	{
		return nullptr;
	}
	TArray<USceneComponent*> Components;
	Wall->GetComponents<USceneComponent>(Components);
	for (USceneComponent* Component : Components)
	{
		if (IsValid(Component) && Component->ComponentHasTag(HingeTag))
		{
			return Component;
		}
	}
	return nullptr;
}

void UWB_HingedDoorComponent::FreezeBody(UPrimitiveComponent* Body) const
{
	if (IsValid(Body))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Body->SetSimulatePhysics(false);
	}
}

bool UWB_HingedDoorComponent::InitializeDoor()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Body = GetBody();
	UPhysicsConstraintComponent* Constraint = GetConstraint();

	if (!Profile || !IsValid(Body) || !IsValid(Constraint))
	{
		UE_LOG(LogWBHingedDoor, Warning, TEXT("%s: the hinged door needs a Profile (DA_HingedDoor_Default), a Door Body and a Hinge Constraint. Door frozen."),
			*GetNameSafe(Owner));
		FreezeBody(Body);
		return false;
	}

	// Hinge: the wall pivot when there is one, otherwise the constraint as placed in the door Blueprint.
	if (USceneComponent* Pivot = FindHingePivot())
	{
		Constraint->SetWorldLocationAndRotation(Pivot->GetComponentLocation(), Pivot->GetComponentQuat());
	}
	else
	{
		UE_LOG(LogWBHingedDoor, Warning, TEXT("%s: no component tagged %s on the wall it is attached to (%s). The hinge stays where the constraint is placed in the door Blueprint."),
			*GetNameSafe(Owner), *HingeTag.ToString(), *GetNameSafe(Owner ? Owner->GetAttachParentActor() : nullptr));
	}

	// Body: mass and damping from the profile.
	Body->SetMassOverrideInKg(NAME_None, Profile->DoorMass, true);
	Body->SetAngularDamping(Profile->AngularDamping);
	Body->SetLinearDamping(Profile->LinearDamping);

	// Hinge: no translation, rotation around the constraint Z axis only (Swing 1), within the swing limit.
	Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, Profile->SwingLimitAngle);
	Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
	Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
	Constraint->SetDisableCollision(true);
	Constraint->SetProjectionEnabled(true);

	// Saloon spring: orientation drive toward closed (target 0), damped by the velocity drive (target 0).
	Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	Constraint->SetAngularDriveAccelerationMode(true);
	Constraint->SetOrientationDriveTwistAndSwing(false, Profile->bUseSpring);
	Constraint->SetAngularVelocityDriveTwistAndSwing(false, Profile->bUseSpring);
	Constraint->SetAngularOrientationTarget(FRotator::ZeroRotator);
	Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
	Constraint->SetAngularDriveParams(Profile->bUseSpring ? Profile->SpringStiffness : 0.f, Profile->bUseSpring ? Profile->SpringDamping : 0.f, 0.f);

	// Held to a fixed point in space (no second body): the wall never moves.
	Constraint->SetConstrainedComponents(Body, NAME_None, nullptr, NAME_None);

	bTornOff = false;
	bLocked = false;
	CurrentLockHealth = 0.f;
	bInitialized = true;
	if (bStartsLocked)
	{
		SetLocked(true);
	}
	return true;
}

void UWB_HingedDoorComponent::SetLocked(bool bNewLocked)
{
	if (bTornOff || !Profile)
	{
		return;
	}
	if (bNewLocked && !Profile->bCanBeLocked)
	{
		UE_LOG(LogWBHingedDoor, Warning, TEXT("%s: Starts Locked is on, but the profile %s cannot be locked (Can Be Locked off). Door left unlocked."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Profile));
		return;
	}

	bLocked = bNewLocked;
	UPrimitiveComponent* Body = GetBody();
	if (bLocked)
	{
		CurrentLockHealth = Profile->LockHealth;
		FreezeBody(Body);
	}
	else if (IsValid(Body))
	{
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();
	}
}

bool UWB_HingedDoorComponent::ApplyLockDamage(float Damage, FVector ImpactLocation, FVector ImpactNormal)
{
	if (!bLocked || bTornOff || Damage <= 0.f)
	{
		return false;
	}
	CurrentLockHealth -= Damage;
	if (CurrentLockHealth > 0.f)
	{
		return false;
	}
	KickOpen(ImpactLocation, ImpactNormal);
	return true;
}

void UWB_HingedDoorComponent::KickOpen(FVector ImpactLocation, FVector ImpactNormal)
{
	if (bTornOff || !Profile)
	{
		return;
	}
	bLocked = false;
	CurrentLockHealth = 0.f;

	UPrimitiveComponent* Body = GetBody();
	UPhysicsConstraintComponent* Constraint = GetConstraint();
	if (!IsValid(Body) || !IsValid(Constraint))
	{
		return;
	}
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();

	// Swing away from the hit: sign of the torque of a push along -Normal, applied at the impact point, around the hinge axis.
	const FVector HingeAxis = Constraint->GetUpVector();
	const FVector Lever = ImpactLocation - Constraint->GetComponentLocation();
	const float Torque = FVector::DotProduct(FVector::CrossProduct(Lever, -ImpactNormal.GetSafeNormal()), HingeAxis);
	const float Sign = Torque < 0.f ? -1.f : 1.f;
	Body->SetPhysicsAngularVelocityInDegrees(HingeAxis * Profile->KickOpenAngularSpeed * Sign);
}

void UWB_HingedDoorComponent::TearOff(FVector ImpactLocation, FVector ImpactNormal)
{
	if (bTornOff)
	{
		return;
	}
	bTornOff = true;
	bLocked = false;
	CurrentLockHealth = 0.f;

	if (UPhysicsConstraintComponent* Constraint = GetConstraint())
	{
		Constraint->BreakConstraint();
	}
	UPrimitiveComponent* Body = GetBody();
	if (!IsValid(Body))
	{
		return;
	}
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	const FVector Push = -ImpactNormal.GetSafeNormal();
	if (Profile && Profile->TearOffSpeed > 0.f && !Push.IsNearlyZero())
	{
		Body->AddImpulse(Push * Profile->TearOffSpeed, NAME_None, true);
	}
}

void UWB_HingedDoorComponent::OnParentDamaged_Implementation(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage)
{
	// The door ignores the shakes of its wall: only its own hits move it or break its lock.
}

void UWB_HingedDoorComponent::OnParentDestroyed_Implementation(AActor* Parent)
{
	// The wall is gone: the door falls off its hinges, without a push.
	TearOff(FVector::ZeroVector, FVector::ZeroVector);
}
