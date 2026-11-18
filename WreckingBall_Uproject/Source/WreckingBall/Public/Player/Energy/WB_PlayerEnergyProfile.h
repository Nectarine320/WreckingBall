#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerEnergyProfile.generated.h"

/**
 * Every energy setting of the player in one place (DA_PlayerEnergy_Default, lot P of Phase C): sprint, recharge, exhaustion and the cost of each action.
 * Energy amounts are fractions of the energy bar (0..1). Read by BP_GenericCharacter (sprint Timelines, ConsumeActionEnergy) through
 * UWB_PlayerStateComponent::EnergyProfile or UWB_PlayerProfileLibrary::GetPlayerEnergyProfile.
 * Same rule for every cost: allowed with less energy than the cost (the bar is emptied, the player becomes Exhausted), refused while Exhausted.
 * Formerly UWB_PlayerActionCostProfile (class redirect in DefaultEngine.ini).
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerEnergyProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Sprint ----

	/** Seconds of sprint a full energy bar gives (the bar empties in this time while sprinting). The recharge from empty to full takes the same time (same Timeline). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Sprint", meta = (ClampMin = "0.1", Units = "Seconds"))
	float SprintDuration = 5.f;

	/** Top sprint speed, as a multiplier of the walk speed (Walk Speed Default x this, capped by Walk Speed Max). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Sprint", meta = (ClampMin = "1"))
	float SprintSpeedMultiplier = 2.f;

	/** Seconds the speed takes to rise from walk to sprint speed when the sprint starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Sprint", meta = (ClampMin = "0.01", Units = "Seconds"))
	float SprintStartBlendTime = 0.5f;

	/** Seconds the speed takes to fall back to walk speed when the sprint stops (released, idle or bar empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Sprint", meta = (ClampMin = "0.01", Units = "Seconds"))
	float SprintStopBlendTime = 0.25f;

	// ---- Recharge ----

	/** Seconds without spending energy before the bar starts to recharge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Recharge", meta = (ClampMin = "0", Units = "Seconds"))
	float EnergyRecoveryDelay = 1.25f;

	// ---- Exhaustion ----

	/** Seconds the player stays exhausted (slow walk, no sprint, no Dash) after the bar is emptied, before the recharge starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Exhaustion", meta = (ClampMin = "0", Units = "Seconds"))
	float ExhaustionDuration = 3.f;

	/** Walk speed in cm/s while exhausted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Exhaustion", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ExhaustedWalkSpeed = 300.f;

	// ---- Action costs ----

	/** Energy used by one Dash (0.4 = 40% of the bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Costs", meta = (ClampMin = "0", ClampMax = "1"))
	float DashEnergyCost = 0.4f;

	/** Energy used when the defensive dive starts (0.3 = 30% of the bar). A dive that touches the ball costs only this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Costs", meta = (ClampMin = "0", ClampMax = "1"))
	float DiveStartEnergyCost = 0.3f;

	/** Extra energy used when the dive lands without touching the ball (0.3: a missed dive costs 60% in all). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy|Costs", meta = (ClampMin = "0", ClampMax = "1"))
	float DiveMissEnergyCost = 0.3f;
};
