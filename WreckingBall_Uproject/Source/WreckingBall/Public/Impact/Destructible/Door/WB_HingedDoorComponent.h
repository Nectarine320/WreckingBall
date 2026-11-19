#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Impact/Destructible/WB_DestructibleDependent.h"
#include "WB_HingedDoorComponent.generated.h"

class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;
class UWB_HingedDoorProfile;

/**
 * Drives a hinged door (BP_Destructible_HingedDoor_Base): sets up its Physics Constraint from the profile (swing limit,
 * saloon spring), handles the lock (a locked door is frozen and acts like a wall, its lock breaks under damage and the door
 * swings open) and tears the door off its hinges (constraint broken, the door falls as one physics block).
 * The constraint holds the door to a fixed point in space (no second body): the hinge pivot of the wall the door is attached
 * to (a scene component tagged HingeTag, Z = hinge axis) or, without one, where the constraint is placed in the door Blueprint.
 * As a dependent of its wall, the door is torn off when the wall is destroyed.
 */
UCLASS(ClassGroup = (WreckingBall), Blueprintable, meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_HingedDoorComponent : public UActorComponent, public IWB_DestructibleDependent
{
	GENERATED_BODY()

public:
	UWB_HingedDoorComponent();

	/** Tuning of the door (hinge, spring, lock, tear off). Required: without it, the door stays frozen and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor")
	TObjectPtr<UWB_HingedDoorProfile> Profile;

	/** The door body that swings: a primitive component of the owner, with Simulate Physics on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HingedDoor", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.PrimitiveComponent"))
	FComponentReference DoorBody;

	/** The Physics Constraint of the owner that holds DoorBody on its hinge. Attach it to the root, not to DoorBody. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HingedDoor", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.PhysicsConstraintComponent"))
	FComponentReference HingeConstraint;

	/** Tag of the scene component, on the actor the door is attached to (wall prefab), that marks the hinge: location = hinge, Z axis = hinge axis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HingedDoor")
	FName HingeTag = TEXT("DoorHinge");

	/** True: the door starts locked (frozen, acts like a wall until its lock breaks). Needs a profile with Can Be Locked. Set it per door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor")
	bool bStartsLocked = false;

	/** Sets up the hinge from the profile. Called automatically at BeginPlay. Returns false (door frozen, warning logged) without a profile, a door body or a constraint. */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	bool InitializeDoor();

	/**
	 * Damages the lock of a locked door: when its health reaches 0, the door is kicked open (see KickOpen).
	 * Call it with every damage the door receives (OnDamaged). Returns true if the lock broke on this hit. Does nothing if the door is not locked.
	 */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	bool ApplyLockDamage(float Damage, FVector ImpactLocation, FVector ImpactNormal);

	/** Locks or unlocks the door. Locking freezes it where it is. Ignored once the door is torn off, or if the profile cannot be locked. */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	void SetLocked(bool bNewLocked);

	/** Unlocks the door and swings it open at the profile KickOpenAngularSpeed, away from the hit. ImpactNormal points toward the attacker (impact data Normal). */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	void KickOpen(FVector ImpactLocation, FVector ImpactNormal);

	/** Breaks the hinge: the door falls as one physics block, pushed away from the hit at the profile TearOffSpeed (zero normal: no push). Once only. */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	void TearOff(FVector ImpactLocation, FVector ImpactNormal);

	/** True while the door is locked. */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	bool IsLocked() const { return bLocked; }

	/** True once the door has been torn off its hinges. */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	bool IsTornOff() const { return bTornOff; }

	/** Remaining health of the lock (0 when the door is not locked). */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	float GetLockHealth() const { return bLocked ? CurrentLockHealth : 0.f; }

	// IWB_DestructibleDependent: the wall shakes are ignored, the door is torn off when the wall is destroyed.
	virtual void OnParentDamaged_Implementation(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage) override;
	virtual void OnParentDestroyed_Implementation(AActor* Parent) override;

protected:
	virtual void BeginPlay() override;

private:
	UPrimitiveComponent* GetBody() const;
	UPhysicsConstraintComponent* GetConstraint() const;
	USceneComponent* FindHingePivot() const;
	void FreezeBody(UPrimitiveComponent* Body) const;

	bool bInitialized = false;
	bool bLocked = false;
	bool bTornOff = false;
	float CurrentLockHealth = 0.f;
};
