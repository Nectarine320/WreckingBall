#include "Impact/Destructible/Door/WB_HingedDoorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Impact/Destructible/Door/WB_HingedDoorProfile.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Player/Hit/WB_PlayerHitComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBHingedDoor, Log, All);

namespace WBHingedDoor
{
	/** Below this opening angle in degrees, the door counts as closed: any motion is an opening. Technical tolerance, not a tuning value. */
	static constexpr float ClosedAngleTolerance = 2.f;
}

UWB_HingedDoorComponent::UWB_HingedDoorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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

	RestRotation = Body->GetComponentQuat();
	RestLocation = Body->GetComponentLocation();
	bForcedOpening = false;
	ForcedVictims.Reset();
	Body->OnComponentHit.AddUniqueDynamic(this, &UWB_HingedDoorComponent::HandleBodyHit);

	bTornOff = false;
	bLocked = false;
	bSlammed = false;
	bSlamSpent = false;
	CurrentLockHealth = 0.f;
	PriorityDasher.Reset();
	ProtectedUntil.Reset();
	bInitialized = true;
	SetComponentTickEnabled(true);
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

	bForcedOpening = false;
	bLocked = bNewLocked;
	bSlammed = false;
	UPrimitiveComponent* Body = GetBody();
	if (bLocked)
	{
		CurrentLockHealth = Profile->LockHealth;
		FreezeBody(Body);
		SetComponentTickEnabled(false);
	}
	else
	{
		if (IsValid(Body))
		{
			Body->SetSimulatePhysics(true);
			Body->WakeAllRigidBodies();
		}
		SetComponentTickEnabled(bInitialized);
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
	// The lock breaks: the door comes off its hinges and becomes a loose physics block (user decision 2026-11-19, replaces the "kicked open" stage).
	TearOff(ImpactLocation, ImpactNormal);
	return true;
}

void UWB_HingedDoorComponent::TearOff(FVector ImpactLocation, FVector ImpactNormal)
{
	if (bTornOff)
	{
		return;
	}
	bTornOff = true;
	bForcedOpening = false;
	bLocked = false;
	bSlammed = false;
	CurrentLockHealth = 0.f;
	SetComponentTickEnabled(false);

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

void UWB_HingedDoorComponent::GetSwing(float& OutAngle, float& OutAngularSpeed) const
{
	OutAngle = 0.f;
	OutAngularSpeed = 0.f;
	const UPrimitiveComponent* Body = GetBody();
	const UPhysicsConstraintComponent* Constraint = GetConstraint();
	if (!IsValid(Body) || !IsValid(Constraint))
	{
		return;
	}
	const FVector Axis = Constraint->GetUpVector();
	// Twist of the rotation since rest, around the hinge axis.
	const FQuat Delta = Body->GetComponentQuat() * RestRotation.Inverse();
	const float Projection = FVector::DotProduct(FVector(Delta.X, Delta.Y, Delta.Z), Axis);
	OutAngle = FRotator::NormalizeAxis(FMath::RadiansToDegrees(2.f * FMath::Atan2(Projection, Delta.W)));
	OutAngularSpeed = FVector::DotProduct(Body->GetPhysicsAngularVelocityInDegrees(), Axis);
}

void UWB_HingedDoorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Profile || bTornOff || bLocked)
	{
		bSlammed = false;
		return;
	}
	if (bForcedOpening)
	{
		// Driven by code: dangerous by definition (TickForcedOpening knocks Down the players in the way).
		bSlammed = true;
		TickForcedOpening(DeltaTime);
		return;
	}

	float Angle = 0.f;
	float Speed = 0.f;
	GetSwing(Angle, Speed);
	// Opening: moving away from closed (same sign as the angle), or any motion while (almost) closed.
	const bool bOpening = FMath::Abs(Angle) <= WBHingedDoor::ClosedAngleTolerance ? !FMath::IsNearlyZero(Speed) : Angle * Speed > 0.f;
	if (!bOpening)
	{
		// Swinging back toward closed: harmless again, and the next opening can slam someone.
		bSlamSpent = false;
	}
	bSlammed = bOpening && !bSlamSpent && FMath::Abs(Speed) >= Profile->SlamMinAngularSpeed;
}

void UWB_HingedDoorComponent::StartForcedOpening(FVector PushLocation, FVector PushDirection)
{
	UPrimitiveComponent* Body = GetBody();
	UPhysicsConstraintComponent* Constraint = GetConstraint();
	if (!Profile || !IsValid(Body) || !IsValid(Constraint))
	{
		return;
	}

	// Side of the swing: sign of the torque of the push, applied at the push point, around the hinge axis.
	const FVector HingeAxis = Constraint->GetUpVector();
	const FVector Lever = PushLocation - Constraint->GetComponentLocation();
	const float Torque = FVector::DotProduct(FVector::CrossProduct(Lever, PushDirection.GetSafeNormal()), HingeAxis);
	const float Sign = Torque < 0.f ? -1.f : 1.f;
	const float Target = Sign * FMath::Min(Profile->DashOpenAngle, Profile->SwingLimitAngle);

	float Angle = 0.f;
	float Speed = 0.f;
	GetSwing(Angle, Speed);
	// Already open at least that far on that side: nothing to force.
	if (Angle * Sign >= FMath::Abs(Target))
	{
		return;
	}

	ForcedAngle = Angle;
	ForcedTargetAngle = Target;
	ForcedVictims.Reset();
	bForcedOpening = true;
	bSlamSpent = false;
	// Kinematic while the code drives it: nothing stops the door, it pushes the simulated objects in its way.
	Body->SetSimulatePhysics(false);
}

void UWB_HingedDoorComponent::TickForcedOpening(float DeltaTime)
{
	UPrimitiveComponent* Body = GetBody();
	UPhysicsConstraintComponent* Constraint = GetConstraint();
	UWorld* World = GetWorld();
	if (!Profile || !IsValid(Body) || !IsValid(Constraint) || !World)
	{
		EndForcedOpening();
		return;
	}

	ForcedAngle = FMath::FInterpConstantTo(ForcedAngle, ForcedTargetAngle, DeltaTime, Profile->DashKickAngularSpeed);

	// Rotate the body around the hinge (not teleported: the physics sees the motion and pushes what is in the way).
	const FVector Pivot = Constraint->GetComponentLocation();
	const FVector Axis = Constraint->GetUpVector();
	const FQuat Swing(Axis, FMath::DegreesToRadians(ForcedAngle));
	const FQuat NewRotation = Swing * RestRotation;
	const FVector NewLocation = Pivot + Swing.RotateVector(RestLocation - Pivot);
	Body->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);

	// Players in the way: knocked Down once each, pushed the way the door moves at their position.
	TArray<FOverlapResult> Overlaps;
	FComponentQueryParams Params(SCENE_QUERY_STAT(WBHingedDoorForcedOpening), GetOwner());
	Body->ComponentOverlapMulti(Overlaps, World, NewLocation, NewRotation, ECC_Pawn, Params);
	const float Side = ForcedTargetAngle < ForcedAngle ? -1.f : 1.f;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!IsValid(Victim) || ForcedVictims.Contains(Victim) || IsProtected(Victim))
		{
			continue;
		}
		UWB_PlayerHitComponent* PlayerHit = Victim->FindComponentByClass<UWB_PlayerHitComponent>();
		if (!PlayerHit)
		{
			continue;
		}
		ForcedVictims.Add(Victim);
		const FVector Push = (FVector::CrossProduct(Axis, Victim->GetActorLocation() - Pivot) * Side).GetSafeNormal2D();
		PlayerHit->ReceiveSlamHit(GetOwner(), Push);
	}

	if (FMath::IsNearlyEqual(ForcedAngle, ForcedTargetAngle))
	{
		EndForcedOpening();
	}
}

void UWB_HingedDoorComponent::EndForcedOpening()
{
	if (!bForcedOpening)
	{
		return;
	}
	bForcedOpening = false;
	ForcedVictims.Reset();
	// The slam of this opening is over: the door is harmless until its next fast opening.
	bSlamSpent = true;
	bSlammed = false;
	if (UPrimitiveComponent* Body = GetBody())
	{
		if (!bTornOff && !bLocked)
		{
			Body->SetSimulatePhysics(true);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Body->WakeAllRigidBodies();
		}
	}
}

bool UWB_HingedDoorComponent::IsProtected(const AActor* Actor) const
{
	const double* Until = ProtectedUntil.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Actor)));
	const UWorld* World = GetWorld();
	return Until && World && World->GetTimeSeconds() <= *Until;
}

EWB_DoorDashResult UWB_HingedDoorComponent::TryDashThrough(AActor* Dasher, FVector DashDirection, FVector ImpactLocation, float ImpactStrength)
{
	UPrimitiveComponent* Body = GetBody();
	const UWorld* World = GetWorld();
	if (!Profile || !IsValid(Dasher) || !IsValid(Body) || !World)
	{
		return EWB_DoorDashResult::Recoil;
	}
	FVector Direction = DashDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Dasher->GetActorForwardVector().GetSafeNormal2D();
	}

	// Loose door (off its hinges): a physics block pushed along the Dash.
	if (bTornOff)
	{
		Body->AddImpulse(Direction * Profile->LooseDoorDashPushSpeed, NAME_None, true);
		return EWB_DoorDashResult::PassThrough;
	}
	if (bLocked || !bInitialized)
	{
		return EWB_DoorDashResult::Recoil;
	}

	const double Now = World->GetTimeSeconds();
	const bool bPriorityActive = PriorityDasher.IsValid() && (Now - PriorityTime) <= Profile->DashPriorityWindow;

	// The priority dasher hits the door again during the same Dash: keep going.
	if (bPriorityActive && PriorityDasher.Get() == Dasher)
	{
		return EWB_DoorDashResult::PassThrough;
	}

	// Take the priority: nobody has it, or another dasher took it in this very frame with a weaker impact.
	const bool bSameFrame = bPriorityActive && PriorityFrame == GFrameCounter;
	if (!bPriorityActive || (bSameFrame && ImpactStrength > PriorityStrength))
	{
		if (bSameFrame)
		{
			// The weaker dasher of this frame loses its protection: the door now swings toward them.
			ProtectedUntil.Remove(PriorityDasher);
		}
		PriorityDasher = Dasher;
		PriorityFrame = GFrameCounter;
		PriorityTime = Now;
		PriorityStrength = ImpactStrength;
		PriorityDirection = Direction;
		ProtectedUntil.Add(Dasher, Now + Profile->DasherProtectionDuration);
		StartForcedOpening(ImpactLocation, Direction);
		return EWB_DoorDashResult::PassThrough;
	}

	// Another dasher has the priority. Same side: goes through behind them, protected too.
	if (FVector::DotProduct(Direction, PriorityDirection) > 0.f)
	{
		ProtectedUntil.Add(Dasher, Now + Profile->DasherProtectionDuration);
		return EWB_DoorDashResult::PassThrough;
	}

	// Other side: the door swings into this dasher and knocks them Down, pushed the way the door moves.
	if (UWB_PlayerHitComponent* PlayerHit = Dasher->FindComponentByClass<UWB_PlayerHitComponent>())
	{
		if (PlayerHit->ReceiveSlamHit(GetOwner(), PriorityDirection))
		{
			bSlamSpent = true;
			return EWB_DoorDashResult::KnockedDown;
		}
	}
	return EWB_DoorDashResult::Recoil;
}

void UWB_HingedDoorComponent::HandleBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// During the forced opening, TickForcedOpening finds the players in the way itself.
	if (bForcedOpening || !bSlammed || bSlamSpent || !IsValid(OtherActor) || IsProtected(OtherActor))
	{
		return;
	}
	UWB_PlayerHitComponent* PlayerHit = OtherActor->FindComponentByClass<UWB_PlayerHitComponent>();
	UPrimitiveComponent* Body = GetBody();
	if (!PlayerHit || !IsValid(Body))
	{
		return;
	}
	// Pushed the way the door moves at the contact point (flat).
	FVector Push = Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint).GetSafeNormal2D();
	if (Push.IsNearlyZero())
	{
		Push = (OtherActor->GetActorLocation() - Body->GetComponentLocation()).GetSafeNormal2D();
	}
	if (PlayerHit->ReceiveSlamHit(GetOwner(), Push))
	{
		bSlamSpent = true;
		bSlammed = false;
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
