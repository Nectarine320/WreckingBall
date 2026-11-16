#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WB_PlayerStateProfile.generated.h"

/**
 * Player state rules (Phase C, step 9): which state can follow which, hand rules, and fallback durations.
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

	/** Seconds before the ball leaves the foot and Kicking ends, used while the kick animation has no Release notify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float KickFallbackDuration = 0.3f;

	/** Seconds before the held object leaves the hands and Throwing ends, used while the throw animation has no Release notify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float ThrowFallbackDuration = 0.3f;

	/** Seconds the scorer stays in Celebrating after a goal. The other players sulk for the same duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Durations", meta = (ClampMin = "0", Units = "Seconds"))
	float CelebrationDuration = 2.f;

	/** Walk speed multiplier while Kicking or Throwing (1 = normal speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionMoveSpeedMultiplier = 1.f;
};
