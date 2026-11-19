#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_HeldObjectMotionComponent.generated.h"

class ACharacter;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class UWB_HeldObjectMotionProfile;
class UWB_PlayerStateComponent;

/**
 * Moves the target of the character's Physics Handle while it holds an object (UI 3D devis, lot A): carry point in front of the player,
 * sway, velocity lag, weight sag, throw windup arc. Replaces the 0.001 s looping timer of BPC_GrabObject (UpdateGrabItemPosition).
 * Ticks once per frame, before physics, and only while holding: BPC_GrabObject calls StartHolding after the grab and StopHolding on release.
 * Stops by itself when the player no longer holds anything (UWB_PlayerStateComponent).
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_HeldObjectMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_HeldObjectMotionComponent();

	/** Motion settings (DA_HeldObjectMotion_Default). Required: empty, StartHolding does nothing and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject")
	TObjectPtr<UWB_HeldObjectMotionProfile> Profile;

	/**
	 * Starts moving the Physics Handle target: call right after the Physics Handle grabbed Held.
	 * bAtPoint: grabbed at a point (Grab.Mode.AtPoint), only the location is driven, without sway. bIsBall: carried at the ball height.
	 */
	UFUNCTION(BlueprintCallable, Category = "HeldObject")
	void StartHolding(UPrimitiveComponent* Held, bool bAtPoint, bool bIsBall);

	/** Stops moving the target (the object was released, thrown or dropped). */
	UFUNCTION(BlueprintCallable, Category = "HeldObject")
	void StopHolding();

	/** True between StartHolding and StopHolding. */
	UFUNCTION(BlueprintPure, Category = "HeldObject")
	bool IsHolding() const { return HeldComponent.IsValid(); }

	/** World location the Physics Handle target was last moved to (carry point with sway and lag). */
	UFUNCTION(BlueprintPure, Category = "HeldObject")
	FVector GetTargetLocation() const { return LastTargetLocation; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerStateComponent> PlayerState;

	bool bHeldAtPoint = false;
	bool bHeldIsBall = false;

	/** Phase in degrees of the position and rotation sways. */
	float PositionSwayPhase = 0.f;
	float RotationSwayPhase = 0.f;

	/** Weight sag spring: current offset and velocity. */
	FVector WeightOffset = FVector::ZeroVector;
	FVector WeightVelocity = FVector::ZeroVector;

	FRotator Tilt = FRotator::ZeroRotator;
	FVector LastTargetLocation = FVector::ZeroVector;

	/** Desired carry point before sway and lag (with the throw windup arc). */
	FVector ComputeCarryPoint(const UPrimitiveComponent* Held) const;
};
