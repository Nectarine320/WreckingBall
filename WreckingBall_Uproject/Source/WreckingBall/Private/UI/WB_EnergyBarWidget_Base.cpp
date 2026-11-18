#include "UI/WB_EnergyBarWidget_Base.h"

#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/WB_EnergyBarVisualProfile.h"

namespace
{
	const FName PercentParam(TEXT("Percent"));
	const FName ConsumedPercentParam(TEXT("ConsumedPercent"));
	const FName ColorParam(TEXT("Color"));
	const FName ConsumedColorParam(TEXT("ConsumedColor"));
}

void UWB_EnergyBarWidget_Base::SetBarMaterial(UMaterialInstanceDynamic* InMaterial)
{
	BarMaterial = InMaterial;
}

double UWB_EnergyBarWidget_Base::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

bool UWB_EnergyBarWidget_Base::UpdateEnergyBar(float Percent, float ConsumedPercent, bool bExhausted)
{
	const UWB_EnergyBarVisualProfile* Profile = VisualProfile.Get();
	if (!Profile)
	{
		if (!bWarnedNoProfile)
		{
			bWarnedNoProfile = true;
			UE_LOG(LogTemp, Warning, TEXT("%s: no VisualProfile (DA_EnergyBarVisual expected). The energy bar is not updated."), *GetName());
		}
		return true;
	}
	const double Now = GetNow();
	Percent = FMath::Clamp(Percent, 0.f, 1.f);

	// Exhausted phase: from empty until full again. Reaching full starts the white "refilled" blink.
	if (bExhausted && Percent < 1.f)
	{
		bInExhaustedPhase = true;
		RefilledStartTime = -1.0;
	}
	else if (bInExhaustedPhase && Percent >= 1.f)
	{
		bInExhaustedPhase = false;
		RefilledStartTime = Now;
	}

	// Same blend as the former Blueprint (plain RGB lerp, red to green).
	FLinearColor Color = FMath::Lerp(Profile->EmptyColor, Profile->FullColor, Percent);
	FLinearColor ConsumedColor = FLinearColor::White;
	bool bCanHide = true;

	if (bInExhaustedPhase)
	{
		// Slow sine blink between two dark reds, one cycle every ExhaustedBlinkPeriod. The whole ring blinks:
		// the recharged part in the blink color, the rest of the ring darker (so an empty bar still shows).
		const float Wave = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * static_cast<float>(Now) / FMath::Max(Profile->ExhaustedBlinkPeriod, 0.02f));
		Color = FMath::Lerp(Profile->ExhaustedColorA, Profile->ExhaustedColorB, Wave);
		ConsumedColor = Color * Profile->ExhaustedEmptyBrightness;
		ConsumedColor.A = 1.f;
		ConsumedPercent = 1.f;
	}
	else if (RefilledStartTime >= 0.0)
	{
		const float Elapsed = static_cast<float>(Now - RefilledStartTime);
		if (Elapsed < Profile->RefilledFlashDuration)
		{
			// One sine bump: full color -> white -> full color.
			const float Bump = FMath::Sin(PI * Elapsed / FMath::Max(Profile->RefilledFlashDuration, KINDA_SMALL_NUMBER));
			Color = FMath::Lerp(Profile->FullColor, Profile->RefilledFlashColor, Bump);
		}
		else
		{
			Color = Profile->FullColor;
		}
		bCanHide = Elapsed >= Profile->RefilledFlashDuration + Profile->RefilledHoldDuration;
		if (bCanHide)
		{
			RefilledStartTime = -1.0;
		}
	}

	if (BarMaterial)
	{
		BarMaterial->SetScalarParameterValue(PercentParam, Percent);
		BarMaterial->SetScalarParameterValue(ConsumedPercentParam, FMath::Max(ConsumedPercent, Percent));
		BarMaterial->SetVectorParameterValue(ColorParam, Color);
		BarMaterial->SetVectorParameterValue(ConsumedColorParam, ConsumedColor);
	}
	return bCanHide;
}
