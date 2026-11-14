#pragma once

#include "CoreMinimal.h"
#include "WB_WobbleTypes.generated.h"

/**
 * Wobble tuning of a destructible, stored in its profile (PDA_DestructibleProfile).
 * The hit damage is mapped between DamageLight and DamageStrong, then each value is lerped between its Light and Strong version.
 * AngleLight or DurationLight at 0 disables the wobble (default: disabled).
 */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_WobbleSettings
{
	GENERATED_BODY()

	/** Peak wobble angle in degrees for a light hit (damage at or below DamageLight). 0 disables the wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0", Units = "Degrees"))
	float AngleLight = 0.f;

	/** Peak wobble angle in degrees for a strong hit (damage at or above DamageStrong). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0", Units = "Degrees"))
	float AngleStrong = 0.f;

	/** Wobble length in seconds for a light hit. 0 disables the wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0", Units = "Seconds"))
	float DurationLight = 0.f;

	/** Wobble length in seconds for a strong hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0", Units = "Seconds"))
	float DurationStrong = 0.f;

	/** Oscillations per second for a light hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0"))
	float FrequencyLight = 0.f;

	/** Oscillations per second for a strong hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0"))
	float FrequencyStrong = 0.f;

	/** Damage at or below which the Light values are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0"))
	float DamageLight = 0.f;

	/** Damage at or above which the Strong values are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0"))
	float DamageStrong = 0.f;

	/** When a new hit interrupts a wobble, time in seconds for the old wobble pose to fade out while the new one starts. 0 snaps the old pose away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (ClampMin = "0", Units = "Seconds"))
	float InterruptFadeTime = 0.1f;
};
