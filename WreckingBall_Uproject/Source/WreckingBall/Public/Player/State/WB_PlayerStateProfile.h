#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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

	/** States that can only be entered with empty hands (Player.Holding.None). Default: Dashing (no Dash while holding something). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Transitions", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesRequiringEmptyHands;

	/** States that can only be entered while holding something (Player.Holding is not None). Default: Kicking, Throwing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Transitions", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesRequiringHolding;

	/** States where the move and look inputs have no effect. Default: Disabled, Down, Stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Inputs", meta = (Categories = "Player.State"))
	FGameplayTagContainer StatesBlockingMovement;

	/** States where the sprint input has no effect (a sprint in progress is stopped by the Blueprint). Default: Disabled, Down, Stunned, Celebrating. */
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

	/** Run speed at the start of Celebrating, as a multiplier of the player's default walk speed. The speed ramps up to CelebrationEndSpeedMultiplier over CelebrationDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0"))
	float CelebrationStartSpeedMultiplier = 0.5f;

	/** Run speed at the end of Celebrating, as a multiplier of the player's default walk speed. The ramp accelerates (slow start, fast end). Back to the default walk speed when Celebrating ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0"))
	float CelebrationEndSpeedMultiplier = 2.f;

	/** Move speed multiplier at full kick / throw charge while Kicking or Throwing. The speed goes from 1 (no charge) to this value (full charge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionMoveSpeedMultiplier = 0.5f;
};
