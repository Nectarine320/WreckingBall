#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WB_EnergyBarWidget_Base.generated.h"

class UMaterialInstanceDynamic;

/**
 * Radial energy bar (sprint and Dash energy, WBP_Sprintbar). Drives the bar material (M_RadialProgressBar: Percent, ConsumedPercent, Color):
 * normal color from empty to full, white "used energy" part, slow dark red blink while the bar recharges from empty (exhausted),
 * then a white blink and the full color for a moment before the owner may hide the bar.
 * The owner calls UpdateEnergyBar on its UI refresh timer and hides the bar only when it returns true.
 */
UCLASS(Blueprintable, Abstract)
class WRECKINGBALL_API UWB_EnergyBarWidget_Base : public UUserWidget
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

	/** Color of the blink shown when the bar is full again after being empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled")
	FLinearColor RefilledFlashColor = FLinearColor::White;

	/** Seconds of the white blink when the bar is full again after being empty (one sine bump: full color, white, full color). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled", meta = (ClampMin = "0", Units = "Seconds"))
	float RefilledFlashDuration = 0.25f;

	/** Seconds the full color stays shown after the white blink, before the bar may hide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar|Refilled", meta = (ClampMin = "0", Units = "Seconds"))
	float RefilledHoldDuration = 0.25f;

	/** Material instance of the bar image, created by the Blueprint (PreConstruct) from MI_SprintProgressBar. */
	UFUNCTION(BlueprintCallable, Category = "EnergyBar")
	void SetBarMaterial(UMaterialInstanceDynamic* InMaterial);

	/**
	 * Refreshes the bar. Percent: energy 0..1. ConsumedPercent: end of the white "used energy" part (Percent or more).
	 * bExhausted: the energy is empty or recharging from empty (blinks dark red until full).
	 * Returns true when the owner may hide the bar now (false while the refilled white blink and hold are playing).
	 */
	UFUNCTION(BlueprintCallable, Category = "EnergyBar", meta = (ReturnDisplayName = "CanHide"))
	bool UpdateEnergyBar(float Percent, float ConsumedPercent, bool bExhausted);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarMaterial;

	/** True from the moment the energy is exhausted until the bar is full again. */
	bool bInExhaustedPhase = false;

	/** Game time when the bar became full after being empty (start of the white blink). -1: not playing. */
	double RefilledStartTime = -1.0;

	double GetNow() const;
};
