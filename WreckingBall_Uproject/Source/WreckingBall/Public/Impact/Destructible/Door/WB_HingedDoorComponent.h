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

/** What a Dash into the door must do (answer of TryDashThrough). */
UENUM(BlueprintType)
enum class EWB_DoorDashResult : uint8
{
	/** The door blocks the Dash: locked door. The dasher bounces back as on any surface too heavy to break (TryDashRecoil). */
	Recoil,
	/** The dasher goes through: ignore the door for the rest of the Dash (unlocked door kicked open, or loose door pushed away). */
	PassThrough,
	/** The door, slammed by another dasher, knocked this dasher Down: stop the Dash. */
	KnockedDown
};

/**
 * Drives a hinged door (BP_Destructible_HingedDoor_Base): sets up its Physics Constraint from the profile (swing limit,
 * saloon spring), handles the lock (a locked door is frozen and acts like a wall; when its lock breaks, the door is torn off),
 * the Dash through an unlocked door (priority to the first dasher, who is protected) and the slam (a door opening fast knocks players Down).
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

	/** Tuning of the door (hinge, spring, lock, Dash, slam, tear off). Required: without it, the door stays frozen and a warning is logged. */
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
	 * Damages the lock of a locked door: when its health reaches 0, the door is torn off its hinges (see TearOff).
	 * Call it with every damage the door receives (OnDamaged). Returns true if the lock broke on this hit. Does nothing if the door is not locked.
	 */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	bool ApplyLockDamage(float Damage, FVector ImpactLocation, FVector ImpactNormal);

	/** Locks or unlocks the door. Locking freezes it where it is. Ignored once the door is torn off, or if the profile cannot be locked. */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	void SetLocked(bool bNewLocked);

	/** Breaks the hinge: the door falls as one physics block, pushed away from the hit at the profile TearOffSpeed (zero normal: no push). Once only. */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	void TearOff(FVector ImpactLocation, FVector ImpactNormal);

	/**
	 * A player dashed into the door (sweep hit of BP_GenericCharacter). DashDirection: flat Dash direction; ImpactLocation: sweep impact point;
	 * ImpactStrength: strength of the Dash impact, used to pick the winner when two players dash into the door in the same frame.
	 * Locked door: Recoil. Loose door (torn off): pushed along the Dash, PassThrough. Unlocked door: the first dasher (or the strongest in the same frame)
	 * gets the priority for DashPriorityWindow: the door swings open away from them (slam) and they go through (PassThrough), protected from the door;
	 * a dasher coming from the other side during the window is knocked Down by the door (KnockedDown), one from the same side goes through too.
	 */
	UFUNCTION(BlueprintCallable, Category = "HingedDoor")
	EWB_DoorDashResult TryDashThrough(AActor* Dasher, FVector DashDirection, FVector ImpactLocation, float ImpactStrength);

	/** True while the door is locked. */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	bool IsLocked() const { return bLocked; }

	/** True once the door has been torn off its hinges. */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	bool IsTornOff() const { return bTornOff; }

	/** True while the door swings away from closed faster than the profile SlamMinAngularSpeed: a player it hits is knocked Down. */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	bool IsSlammed() const { return bSlammed; }

	/** Remaining health of the lock (0 when the door is not locked). */
	UFUNCTION(BlueprintPure, Category = "HingedDoor")
	float GetLockHealth() const { return bLocked ? CurrentLockHealth : 0.f; }

	// IWB_DestructibleDependent: the wall shakes are ignored, the door is torn off when the wall is destroyed.
	virtual void OnParentDamaged_Implementation(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage) override;
	virtual void OnParentDestroyed_Implementation(AActor* Parent) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UPrimitiveComponent* GetBody() const;
	UPhysicsConstraintComponent* GetConstraint() const;
	USceneComponent* FindHingePivot() const;
	void FreezeBody(UPrimitiveComponent* Body) const;

	/** Signed opening angle in degrees around the hinge axis (0 = closed) and signed angular speed in degrees per second around the same axis. */
	void GetSwing(float& OutAngle, float& OutAngularSpeed) const;

	/** Starts the forced opening (kinematic) to the profile DashOpenAngle, away from a push along PushDirection applied at PushLocation. */
	void StartForcedOpening(FVector PushLocation, FVector PushDirection);

	/** One step of the forced opening: rotates the body around the hinge, knocks Down the players it overlaps, ends the swing at the target angle. */
	void TickForcedOpening(float DeltaTime);

	/** Ends the forced opening: the door simulates again (the spring brings it back). */
	void EndForcedOpening();

	/** True while Actor cannot be slammed by this door (the priority dasher and same-side dashers, for DasherProtectionDuration). */
	bool IsProtected(const AActor* Actor) const;

	UFUNCTION()
	void HandleBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool bInitialized = false;
	bool bLocked = false;
	bool bTornOff = false;
	float CurrentLockHealth = 0.f;

	/** Transform of the door body at rest (closed), in world space. Reference of the opening angle and of the forced opening. */
	FQuat RestRotation = FQuat::Identity;
	FVector RestLocation = FVector::ZeroVector;

	/** Forced opening in progress (kinematic swing after a Dash): current and target angles in degrees (signed, 0 = closed). */
	bool bForcedOpening = false;
	float ForcedAngle = 0.f;
	float ForcedTargetAngle = 0.f;

	/** Last impact the wall received (OnParentDamaged): when the wall is destroyed, the door is torn off away from it. */
	bool bHasParentImpact = false;
	FVector LastParentImpactLocation = FVector::ZeroVector;
	FVector LastParentImpactNormal = FVector::ZeroVector;

	/** Players already knocked Down by the current forced opening (each one once). */
	TArray<TWeakObjectPtr<AActor>> ForcedVictims;

	/** True while the door opens faster than SlamMinAngularSpeed. */
	bool bSlammed = false;

	/** True once the current slam knocked someone Down: no second victim until the door swings back toward closed. */
	bool bSlamSpent = false;

	/** Dasher who has the priority on the door, the frame and time they claimed it, the strength of their impact and their Dash direction. */
	TWeakObjectPtr<AActor> PriorityDasher;
	uint64 PriorityFrame = 0;
	double PriorityTime = -1.0;
	float PriorityStrength = 0.f;
	FVector PriorityDirection = FVector::ZeroVector;

	/** Actors protected from the slam, with the game time their protection ends. */
	TMap<TWeakObjectPtr<AActor>, double> ProtectedUntil;
};
