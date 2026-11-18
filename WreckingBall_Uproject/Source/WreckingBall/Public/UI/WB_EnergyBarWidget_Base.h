#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WB_EnergyBarWidget_Base.generated.h"

class UMaterialInstanceDynamic;
class UWB_EnergyBarVisualProfile;

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
	/** Look of the bar: colors, exhausted blink, refilled flash (DA_EnergyBarVisual). Required: empty, the bar is not updated and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnergyBar")
	TObjectPtr<UWB_EnergyBarVisualProfile> VisualProfile;

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

	/** Missing VisualProfile already reported. */
	bool bWarnedNoProfile = false;
};
