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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDefenseBallCaught, AActor*, Ball);

/**
 * Dive button (IA_Dive), two moves (user decision 2026-11-17, Docs/Plans/plan-plongeon.md):
 * - Pressed while moving: defensive dive (bodyguard jump) along the stick. When the ball (kicked or not, within MaxBallDistance) has a point of its
 *   predicted path in the stick cone, the dive goes toward that point. Facing the ball, body tilted toward the jump; no control (scripted, like the Dash).
 *   A touch bounces the ball away from the body. Phases: Takeoff, Air (AirDuration), Land (plus MissStunDuration after a miss), GetUp, then Free.
 *   Land and GetUp play at RecoveryPlayRate.
 * - Pressed without moving: defense stance (Player.State.Defending) while the button is held. X pose. A ball within DefenseBallTrackRadius at the start:
 *   one turn to face it (DefenseTurnSpeed), then the facing stays locked (the ball is not followed). Farther: the player keeps its facing.
 *   The move input stays camera-relative (AddDefenseMoveInput: W = screen up, D = screen right, straight lines, whatever the facing); the part along the body
 *   goes at DefenseForwardSpeedMultiplier, the part across it at DefenseSideSpeedMultiplier. The spread body blocks the ball.
 *   Ends on release (StopDefense) or when another state takes over (Dash hit, goal, Disabled).
 * Added to BP_GenericCharacter next to UWB_PlayerStateComponent. The dive energy is handled by the Blueprint from the events
 * (OnDiveStarted, OnDiveLanded, OnDiveFinished) and the costs of UWB_PlayerEnergyProfile. The defense costs nothing.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerDiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerDiveComponent();

	/** Dive and defense tuning (DA_PlayerDive_Default). Required: empty, the dive and the defense are refused and a warning is logged. */
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
	 * Called when a ball (kicked or not, not held) touches the body in the defense stance: the stance has already ended (back to Free).
	 * Bind to the ball pickup (BPC_GrabObject.PickUpSoccerBall): the interception gives the ball to the defender.
	 */
	UPROPERTY(BlueprintAssignable, Category = "PlayerDive|Defense")
	FWB_OnDefenseBallCaught OnDefenseBallCaught;

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
	 * Bot dive request: same as TryStartDive, but WorldDirection (flattened) replaces the stick. Used by the Goalkeeper bot to dive on a shot.
	 * The ball cone and the target search work as for a human. Returns false when refused (state, defense stance on, no profile).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive", meta = (ReturnDisplayName = "Started"))
	bool TryStartDiveToward(FVector WorldDirection);

	/**
	 * Defense stance request (IA_DefensiveDive pressed without moving). Enters Defending (state table, empty hands): X pose, one turn toward a ball within
	 * DefenseBallTrackRadius then a locked facing, camera-relative moves, blocks the ball. Kept until StopDefense (button released) or another state takes over.
	 * Returns false when the state is refused or the profile is missing. Costs no energy.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense", meta = (ReturnDisplayName = "Started"))
	bool TryStartDefense();

	/** Button released (IA_Dive Completed / Canceled): leaves the defense stance, back to Free. Does nothing when not in the stance (a dive goes on). */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense")
	void StopDefense();

	/**
	 * Move input in the defense stance (called by RequestMove instead of Move while IsDefending). Same stick axes as Move, relative to the yaw of the
	 * player's camera (user rule 2026-11-17): W = screen up, S = down, A = left, D = right, in straight lines, whatever the body facing.
	 * The move is then split along the locked body facing: forward / back at DefenseForwardSpeedMultiplier, sideways at DefenseSideSpeedMultiplier.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense")
	void AddDefenseMoveInput(float RightAxis, float ForwardAxis);

	/**
	 * Defense stance only: replaces the locked facing with WorldDirection (flattened). The body turns toward it at DefenseTurnSpeed, then keeps it.
	 * Used by the bots (a Goalkeeper bot faces the ball every frame in its zone). A human stance never calls it, so its single-aim rule is unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerDive|Defense")
	void SetDefenseFacing(FVector WorldDirection);

	/** True while in the defense stance. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive|Defense")
	bool IsDefending() const { return bDefending; }

	/**
	 * Angle in degrees between the move (velocity) and the body facing, -180..180 (0: forward, 90: right, -90: left, 180: back), like CalculateDirection.
	 * For the AnimBP blend of the defense side steps. 0 when not moving.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerDive|Defense")
	float GetDefenseMoveAngle() const;

	/** Phase of the dive (None when not diving). */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	EWB_DivePhase GetDivePhase() const { return Phase; }

	/** True from the takeoff to the end of the get-up. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	bool IsDiving() const { return Phase != EWB_DivePhase::None; }

	/** True when the current (or last) dive touched the ball. */
	UFUNCTION(BlueprintPure, Category = "PlayerDive")
	bool HasTouchedBall() const { return bTouchedBall; }

	/** The dive profile (DiveProfile). Null when it is not assigned (a warning is logged once; the dive and the defense are then refused). */
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

	/** Yaw in degrees added to the mesh at full tilt so it lies on its side (bLieOnSide), chosen at takeoff. 0 when off. */
	float SideYaw = 0.f;

	/** Capsule response to pawns before the dive (the dive passes through the other players). */
	TEnumAsByte<ECollisionResponse> PawnResponseBefore = ECR_Block;

	/** Takeoff flash. */
	FTimerHandle FlashTimer;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FlashShown;

	/** Defense stance: active, locked yaw (toward the ball at the start, or the facing), single turn still running, movement settings restored at the end. */
	bool bDefending = false;
	float DefenseYaw = 0.f;
	bool bDefenseTurning = false;
	bool bOrientRotationToMovementBefore = false;
	bool bUseControllerDesiredRotationBefore = false;
	bool bUseControllerRotationYawBefore = false;

	/** Set while the component itself changes the state (end of the dive), so the state handler does not treat it as an interruption. */
	bool bChangingStateItself = false;

	/** Simulated primitive of a ball actor (root or first simulated primitive). */
	static UPrimitiveComponent* FindBallBody(AActor* Ball);

	/** Body of the closest ball of BallClass within MaxDistance cm of From (simulated body, or root primitive when held). Null: none. */
	UPrimitiveComponent* FindClosestBall(const FVector& From, float MaxDistance) const;

	/** Missing DiveProfile already reported. */
	mutable bool bWarnedNoProfile = false;

	/** Stick given by TryStartDiveToward for the next PlanDive (bots). Zero: the real stick (last move input). */
	FVector StickOverride = FVector::ZeroVector;

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

	/** Defense stance, every frame: single turn toward the ball then locked facing, ball block. */
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
