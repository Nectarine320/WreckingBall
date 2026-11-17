#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerActionCostProfile.generated.h"

/**
 * Energy cost of every player action, as a fraction of the sprint energy bar (0..1). One place to balance them (DA_PlayerActionCost_Default).
 * Read by BP_GenericCharacter (ConsumeActionEnergy) through UWB_PlayerStateComponent::GetActiveActionCostProfile.
 * Same rule for every cost: allowed with less energy than the cost (the bar is emptied, the player becomes Exhausted), refused while Exhausted.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerActionCostProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Energy used by one Dash (0.4 = 40% of the bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionCost|Dash", meta = (ClampMin = "0", ClampMax = "1"))
	float DashEnergyCost = 0.4f;

	/** Energy used when the defensive dive starts (0.3 = 30% of the bar). A dive that touches the ball costs only this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionCost|Dive", meta = (ClampMin = "0", ClampMax = "1"))
	float DiveStartEnergyCost = 0.3f;

	/** Extra energy used when the dive lands without touching the ball (0.3: a missed dive costs 60% in all). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActionCost|Dive", meta = (ClampMin = "0", ClampMax = "1"))
	float DiveMissEnergyCost = 0.3f;
};
