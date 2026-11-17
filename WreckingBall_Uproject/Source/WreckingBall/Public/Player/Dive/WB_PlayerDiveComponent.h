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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDefenseBallBlocked, AActor*, Ball);

/**
 * Dive button (IA_Dive), two moves (user decision 2026-11-17, Docs/Plans/plan-plongeon.md):
 * - Pressed while moving: defensive dive (bodyguard jump) along the stick. When the ball (kicked or not, within MaxBallDistance) has a point of its
 *   predicted path in the stick cone, the dive goes toward that point. Facing the ball, body tilted toward the jump; no control (scripted, like the Dash).
 *   A touch bounces the ball away from the body. Phases: Takeoff, Air (AirDuration), Land (plus MissStunDuration after a miss), GetUp, then Free.
 *   Land and GetUp play at RecoveryPlayRate.
 * - Pressed without moving: defense stance (Player.State.Defending) while the button is held. X pose, keeps the direction the player faces
 *   (no turn toward the ball), and that facing stays locked (straight moves). The move input is read in the player's own axes (AddDefenseMoveInput): A/D side steps, W/S slower forward / back
 *   (DefenseMoveSpeedMultiplier of the state profile, DefenseForwardSpeedScale). The spread body blocks the ball.
 *   Ends on release (StopDefense) or when another state takes over (Dash hit, goal, Disabled).
 * Added to BP_GenericCharacter next to UWB_PlayerStateComponent. The dive energy is handled by the Blueprint from the events
 * (OnDiveStarted, OnDiveLanded, OnDiveFinished) and the costs of UWB_PlayerActionCostProfile. The defense costs nothing.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerDiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerDiveComponent();

	/** Dive and defense tuning (DA_PlayerDive_Default). Empty: the class defaults of UWB_PlayerDiveProfile are used (no ball class: no ball aim, no ball block). */
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

	/** Called when the defense stance blocked a ball (the ball has already bounced off). */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive|Defense")
	FWB_OnDefenseBallBlocked OnDefenseBallBlocked;

	/**
	 * True when the player is pushing the move input (stick or keys: X or Y axis not zero), read from the last move input. Decides what the dive button
	 * does: moving = dive (TryStartDive), not moving = defense stance (TryStartDefense).
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	bool HasMoveInput() const;

	/**
	 * Dive request (IA_Dive pressed while moving). Dives along the stick (last move input). When the closest ball of BallClass within MaxBallDistance
	 * (kicked or not) has a point of its predicted path inside the stick cone (StickConeHalfAngle), the dive goes toward that point instead,
	 * the reachable one closest to the stick first. No ball in the cone: MaxDistance straight along the stick. Then enters Diving (state table, empty hands).
	 * Returns false (nothing happens) when the state is refused. The Blueprint checks the energy (Exhausted) before.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive", meta = (ReturnDisplayName = "Started"))
	bool TryStartDive();

	/**
	 * Defense stance request (IA_Dive pressed without moving). Enters Defending (state table, empty hands): X pose, keeps the current facing, side steps,
	 * blocks the ball. Kept until StopDefense (button released) or another state takes over. Returns false when the state is refused. Costs no energy.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense", meta = (ReturnDisplayName = "Started"))
	bool TryStartDefense();

	/** Button released (IA_Dive Completed / Canceled): leaves the defense stance, back to Free. Does nothing when not in the stance (a dive goes on). */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense")
	void StopDefense();

	/**
	 * Move input in the defense stance (called by RequestMove instead of the camera-relative Move while IsDefending). The axes follow the player's
	 * facing, not the camera (wanted: a player turned 30 degrees has WASD turned 30 degrees too). RightAxis (A/D): side steps; ForwardAxis (W/S):
	 * toward / away from the faced direction, scaled by DefenseForwardSpeedScale. Both are scaled by the state's move speed multiplier.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense")
	void AddDefenseMoveInput(float RightAxis, float ForwardAxis);

	/** True while in the defense stance. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive|Defense")
	bool IsDefending() const { return bDefending; }

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

	/** Defense stance: active, yaw kept when no ball is in range, movement settings restored at the end. */
	bool bDefending = false;
	float DefenseYaw = 0.f;
	bool bOrientRotationToMovementBefore = false;
	bool bUseControllerDesiredRotationBefore = false;
	bool bUseControllerRotationYawBefore = false;

	/** Set while the component itself changes the state (end of the dive), so the state handler does not treat it as an interruption. */
	bool bChangingStateItself = false;

	/** Simulated primitive of a ball actor (root or first simulated primitive). */
	static UPrimitiveComponent* FindBallBody(AActor* Ball);

	/** Body of the closest ball of BallClass within MaxBallDistance of From (simulated body, or root primitive when held). Null: none. */
	UPrimitiveComponent* FindClosestBall(const FVector& From) const;

	/** Picks the target and fills the planned jump (always succeeds: no ball in the cone means straight along the stick). */
	void PlanDive();

	/** Height in cm above the ground of the body center at jump fraction A (0..1). */
	float BodyHeightAt(float A) const;

	/** Applies the tilt and the body height offset to the mesh. */
	void ApplyMeshPose(float TiltAlpha, float BodyOffset);

	/** Moves the capsule along the planned jump at Elapsed. */
	void TickAir(float DeltaTime);

	/** Ball touch test of the stretched body, and bounce. */
	void TestBallTouch();

	/** Bounces Ball off the body at Contact (closest body point), like on a wall moving at BodyVelocity. Returns false when the ball was not moving into the body. */
	bool BounceBall(UPrimitiveComponent* Ball, const FVector& Contact, const FVector& BodyVelocity) const;

	/** Defense stance, every frame: turn toward the ball, ball block. */
	void TickDefense(float DeltaTime);

	/** Leaves the defense stance: restores the rotation settings; Free when bReturnToFree. */
	void EndDefense(bool bReturnToFree);

	/** Touchdown: Land phase, OnDiveLanded. */
	void Touchdown();

	/** Restores the mesh, the collision and the movement; Free when bReturnToFree; OnDiveFinished. */
	void EndDive(bool bReturnToFree);

	void StartFlash();
	void EndFlash();

	UFUNCTION()
	void HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState);
};
