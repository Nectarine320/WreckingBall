#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "WB_PlayerDiveComponent.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class UWB_PlayerDiveProfile;
class UWB_PlayerStateComponent;

/** Where the player is in the defensive dive. Read every frame by the AnimBP to pick the animation. */
UENUM(BlueprintType)
enum class EWB_DivePhase : uint8
{
	/** Not diving. */
	None,
	/** Start of the jump (length of the profile's JumpAnim): leaves the ground, the body tilts. */
	Takeoff,
	/** Rest of the jump, body stretched toward the ball (AirAnim loop). */
	Air,
	/** On the floor after the touchdown: landing, plus the small stun after a miss. */
	Land,
	/** Getting up from the floor, then back to Free. */
	GetUp
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWB_OnDiveStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDiveBallTouched, AActor*, Ball);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDiveLanded, bool, bTouchedBall);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDiveFinished, bool, bTouchedBall);

/**
 * Defensive dive (bodyguard jump): when a ball kicked by the other team is in flight, the player leaps toward a point of the ball's predicted path,
 * in the stick direction, facing the ball, with the body tilted toward the jump. No control during the jump (scripted, like the Dash).
 * A touch bounces the ball away from the body. Phases: Takeoff, Air (AirDuration), Land (plus MissStunDuration after a miss), GetUp, then Free.
 * Added to BP_GenericCharacter next to UWB_PlayerStateComponent (state Player.State.Diving). The energy is handled by the Blueprint
 * from the events (OnDiveStarted, OnDiveLanded, OnDiveFinished) and the costs of UWB_PlayerActionCostProfile.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerDiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerDiveComponent();

	/** Dive tuning (DA_PlayerDive_Default). Empty: the class defaults of UWB_PlayerDiveProfile are used (no ball class: the dive never starts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive")
	TObjectPtr<UWB_PlayerDiveProfile> DiveProfile;

	/** Draws the predicted ball path, the chosen target and the body during the dive (development builds only). Off by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDrawDive = false;

	/** Called when the dive started (state Diving entered, jump planned). Hook for the energy cost and the bar. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive")
	FWB_OnDiveStarted OnDiveStarted;

	/** Called once per dive when the body touches the ball (the ball has already bounced off). */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive")
	FWB_OnDiveBallTouched OnDiveBallTouched;

	/** Called at the touchdown. bTouchedBall false: the dive missed (hook for the extra energy cost). Not called when a hit cut the jump short. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive")
	FWB_OnDiveLanded OnDiveLanded;

	/** Called when the dive is over: after the get-up, or when another state (hit, goal, Disabled) cut it short. Hook to restart the energy recovery. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive")
	FWB_OnDiveFinished OnDiveFinished;

	/**
	 * Player request (IA_Dive). Looks for a ball of the profile's BallClass in flight (MinBallSpeed, MaxBallDistance, kicked by the other team),
	 * predicts its path over AirDuration and picks the point to reach in the stick direction (last move input; released: the whole circle).
	 * No reachable point: dives MaxDistance along the stick (or toward the ball). Then enters Diving (state table, empty hands) and jumps.
	 * Returns false (nothing happens) when there is no ball in flight or the state is refused. The Blueprint checks the energy (Exhausted) before.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive", meta = (ReturnDisplayName = "Started"))
	bool TryStartDive();

	/** Phase of the dive (None when not diving). */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	EWB_DivePhase GetDivePhase() const { return Phase; }

	/** True from the takeoff to the end of the get-up. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	bool IsDiving() const { return Phase != EWB_DivePhase::None; }

	/** True when the current (or last) dive touched the ball. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	bool HasTouchedBall() const { return bTouchedBall; }

	/** The profile in use: DiveProfile, or the UWB_PlayerDiveProfile class defaults when it is empty. Never null. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	const UWB_PlayerDiveProfile* GetActiveDiveProfile() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** State component of the owner, found at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerStateComponent> PlayerState;

	/** Simulated body of the ball the dive aims at. */
	TWeakObjectPtr<UPrimitiveComponent> TargetBall;

	EWB_DivePhase Phase = EWB_DivePhase::None;
	bool bTouchedBall = false;

	/** Seconds since takeoff, and time in the current ground phase. */
	float Elapsed = 0.f;
	float PhaseElapsed = 0.f;
	float LandDuration = 0.f;
	float GetUpDuration = 0.f;

	/** Planned jump: start location (capsule center), flat direction and distance, ground heights at start and end, body height at the top, moment of the top (0..1). */
	FVector StartLocation = FVector::ZeroVector;
	FVector JumpDirection = FVector::ForwardVector;
	float JumpDistance = 0.f;
	float StartGroundZ = 0.f;
	float EndGroundZ = 0.f;
	float PeakBodyHeight = 0.f;
	float PeakFraction = 0.5f;

	/** Horizontal progress lost on a wall: the jump stops moving forward there. */
	bool bBlockedForward = false;
	FVector BlockedFlatLocation = FVector::ZeroVector;

	/** Mesh relative transform before the dive, restored at the end. */
	FVector MeshBaseLocation = FVector::ZeroVector;
	FRotator MeshBaseRotation = FRotator::ZeroRotator;

	/** Body tilt (0..1 of BodyTilt) and body height offset (cm, world Z) applied to the mesh, kept to unwind them during the get-up. */
	float CurrentTiltAlpha = 0.f;
	float CurrentBodyOffset = 0.f;

	/** Capsule response to pawns before the dive (the dive passes through the other players). */
	TEnumAsByte<ECollisionResponse> PawnResponseBefore = ECR_Block;

	/** Takeoff flash. */
	FTimerHandle FlashTimer;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FlashShown;

	/** Set while the component itself changes the state (end of the dive), so the state handler does not treat it as an interruption. */
	bool bChangingStateItself = false;

	/** Simulated primitive of a ball actor (root or first simulated primitive). */
	static UPrimitiveComponent* FindBallBody(AActor* Ball);

	/** True when Ball was last sent by a player of the other team (or by no known team). */
	bool IsEnemyBall(AActor* Ball) const;

	/** Picks the target and fills the planned jump. Returns false when no ball is in flight. */
	bool PlanDive();

	/** Height in cm above the ground of the body center at jump fraction A (0..1). */
	float BodyHeightAt(float A) const;

	/** Applies the tilt and the body height offset to the mesh. */
	void ApplyMeshPose(float TiltAlpha, float BodyOffset);

	/** Moves the capsule along the planned jump at Elapsed. */
	void TickAir(float DeltaTime);

	/** Ball touch test of the stretched body, and bounce. */
	void TestBallTouch();

	/** Touchdown: Land phase, OnDiveLanded. */
	void Touchdown();

	/** Restores the mesh, the collision and the movement; Free when bReturnToFree; OnDiveFinished. */
	void EndDive(bool bReturnToFree);

	void StartFlash();
	void EndFlash();

	UFUNCTION()
	void HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState);
};
