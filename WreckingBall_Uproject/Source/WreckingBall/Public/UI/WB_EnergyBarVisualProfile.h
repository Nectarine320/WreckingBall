#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_EnergyBarVisualProfile.generated.h"

/**
 * Look of the radial energy bar (DA_EnergyBarVisual, lot P of Phase C): colors, exhausted blink, refilled flash.
 * Read by UWB_EnergyBarWidget_Base (WBP_Sprintbar, shown by WBP_PlayerBar). The energy values themselves are in UWB_PlayerEnergyProfile.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_EnergyBarVisualProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Bar color when the energy is full (the color fades from EmptyColor to this one as the energy rises). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Colors")
	FLinearColor FullColor = FLinearColor(0.072337f, 1.f, 0.f, 1.f);

	/** Bar color when the energy is almost empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Colors")
	FLinearColor EmptyColor = FLinearColor(1.f, 0.020186f, 0.f, 1.f);

	/** First color of the blink while the bar recharges from empty (exhausted): dark red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Exhausted")
	FLinearColor ExhaustedColorA = FLinearColor(0.25f, 0.f, 0.f, 1.f);

	/** Second color of the exhausted blink: even darker red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Exhausted")
	FLinearColor ExhaustedColorB = FLinearColor(0.06f, 0.f, 0.f, 1.f);

	/** Seconds of one full sine cycle of the exhausted blink (A to B and back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Exhausted", meta = (ClampMin = "0.02", Units = "Seconds"))
	float ExhaustedBlinkPeriod = 0.25f;

	/** While exhausted, brightness (0..1) of the part of the ring not recharged yet, compared with the blink color: an empty bar still shows, and the refill stays readable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Exhausted", meta = (ClampMin = "0", ClampMax = "1"))
	float ExhaustedEmptyBrightness = 0.4f;

	/** Color of the blink shown when the bar is full again after being empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled")
	FLinearColor RefilledFlashColor = FLinearColor::White;

	/** Seconds of the white blink when the bar is full again after being empty (one sine bump: full color, white, full color). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled", meta = (ClampMin = "0", Units = "Seconds"))
	float RefilledFlashDuration = 0.25f;

	/** Seconds the full color stays shown after the white blink, before the bar may hide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled", meta = (ClampMin = "0", Units = "Seconds"))
	float RefilledHoldDuration = 0.25f;
};
