#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "WB_PlayerStateProfile.generated.h"

/**
 * Player state rules (Phase C, step 9): which state can follow which, hand rules, input filters and durations.
 * The constructor fills the transition table of the Phase C quote, so a new asset (DA_PlayerState_Default) starts ready to use.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerStateProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWB_PlayerStateProfile();

	/** Transition table: for each current state (key), the states the player may enter from it (value). A state missing from the keys allows nothing. Entering the current state again is always refused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Transitions", meta = (Categories = "Player.State", ForceInlineRow))
	TMap<FGameplayTag, FGameplayTagContainer> AllowedTransitions;

	/** States that can only be entered with empty hands (Player.Holding.None). Default: Dashing, Diving, Defending (no Dash, dive or defense while holding something). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Transitions", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesRequiringEmptyHands;

	/** States that can only be entered while holding something (Player.Holding is not None). Default: Kicking, Throwing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Transitions", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesRequiringHolding;

	// ---- Possession (lot 11.1: automatic catch) ----

	/** Ball actors caught automatically by walking into them (BP_PO_Master_Ball_01 and its children). Empty: no automatic catch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Possession")
	TSubclassOf<AActor> BallClass;

	/** Radius in cm of the catch zone in front of the player: a free ball whose center enters it is caught (Free state, empty hands, ball not kicked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Possession", meta = (ClampMin = "0", Units = "Centimeters"))
	float BallCatchRadius = 60.f;

	/** Distance in cm from the capsule center to the center of the catch zone, toward the facing. 0 = centered on the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Possession", meta = (Units = "Centimeters"))
	float BallCatchForwardOffset = 40.f;

	/** Seconds after a kick during which the kicker cannot catch its own ball again (even after a weak kick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Possession", meta = (ClampMin = "0", Units = "Seconds"))
	float KickerRecatchDelay = 0.5f;

	/**
	 * Kicked ball that rebounded (wall, obstacle, player): it can be caught by walking again once slower than this, in cm/s, even above its
	 * KickedReleaseSpeed. 0: off, a rebound changes nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Possession", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float KickedReboundReleaseSpeed = 0.f;

	/** States where the move and look inputs have no effect. Default: Disabled, Down, Stunned, Recoiling, Diving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Inputs", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesBlockingMovement;

	/** States where the sprint input has no effect (a sprint in progress is stopped by the Blueprint). Default: Disabled, Down, Stunned, Celebrating, Recoiling, Diving, Defending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Inputs", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesBlockingSprint;

	/** Seconds Kicking lasts after the ball left the foot, used while the kick animation has no Release notify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float KickFallbackDuration = 0.3f;

	/** Seconds Throwing lasts after the object left the hands, used while the throw animation has no Release notify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float ThrowFallbackDuration = 0.3f;

	/** Seconds the scoring team stays in Celebrating after a goal. The other team sulks for the same duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float CelebrationDuration = 2.f;

	/** Turn speed in degrees per second of the forced run while Celebrating: how fast the running direction follows the stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0", Units = "DegreesPerSecond"))
	float CelebrationTurnRate = 270.f;

	/**
	 * Run speed of the forced run while Celebrating: X = celebration progress (0 = goal, 1 = end of CelebrationDuration), Y = multiplier of the
	 * player's default walk speed. Default: 0.5 at the start, explosive rise to the peak 2 at mid-celebration, gradual fade back to 1 at the end.
	 * End the curve at 1 so the end of the state has no speed jump. Required: an empty curve keeps the default walk speed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement")
	FRuntimeFloatCurve CelebrationSpeedCurve;

	/** Move speed multiplier at full kick / throw charge while Kicking or Throwing. The speed goes from 1 (no charge) to this value (full charge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionMoveSpeedMultiplier = 0.5f;

	/** Move speed multiplier while holding the ball (possession penalty, so the others can catch the carrier). 0.9 = 10 % slower. 1 = no penalty. Never makes the player drop the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float HoldingBallMoveSpeedMultiplier = 0.9f;

	/**
	 * Move speed multiplier while dragging an object held AtPoint (Grab.Mode.AtPoint), by the object mass: X = mass in kg, Y = multiplier (1 = no slowdown).
	 * Read once at the grab (SetHeldObjectMass). Objects held Centered are not slowed by this curve.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement")
	FRuntimeFloatCurve GrabbedMoveSpeedCurve;

	/** Celebrating run speed multiplier at Progress (0..1 of CelebrationDuration), read from CelebrationSpeedCurve. Empty curve: 1. */
	float EvaluateCelebrationSpeed(float Progress) const;
};
