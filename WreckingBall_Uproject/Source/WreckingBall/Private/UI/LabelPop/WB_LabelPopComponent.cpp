#include "UI/LabelPop/WB_LabelPopComponent.h"

#include "Components/Widget.h"
#include "UI/LabelPop/WB_LabelPopProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBLabelPop, Log, All);

UWB_LabelPopComponent::UWB_LabelPopComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Ticks only while a pop plays.
}

void UWB_LabelPopComponent::SetTargetWidget(UWidget* Widget)
{
	if (TargetWidget.Get() == Widget)
	{
		return;
	}
	StopPop();
	TargetWidget = Widget;
}

void UWB_LabelPopComponent::PlayPop()
{
	if (!Profile)
	{
		if (!bWarnedNoProfile)
		{
			bWarnedNoProfile = true;
			UE_LOG(LogWBLabelPop, Warning, TEXT("%s: no Profile on %s (DA_LabelPop_Default expected). The label pop is off."), *GetNameSafe(GetOwner()), *GetName());
		}
		return;
	}
	UWidget* Widget = TargetWidget.Get();
	if (!Widget)
	{
		UE_LOG(LogWBLabelPop, Verbose, TEXT("%s: no target widget yet, pop skipped."), *GetNameSafe(GetOwner()));
		return;
	}
	Widget->SetRenderTransformPivot(Profile->RenderPivot);
	Widget->SetRenderScale(Profile->EvaluateScale(0.f));
	Elapsed = 0.f;
	bPlaying = true;
	SetComponentTickEnabled(true);
}

void UWB_LabelPopComponent::StopPop()
{
	if (UWidget* Widget = TargetWidget.Get())
	{
		Widget->SetRenderScale(FVector2D::UnitVector);
	}
	bPlaying = false;
	SetComponentTickEnabled(false);
}

void UWB_LabelPopComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWidget* Widget = TargetWidget.Get();
	if (!bPlaying || !Widget || !Profile)
	{
		StopPop();
		return;
	}
	Elapsed += DeltaTime;
	const float Progress = FMath::Clamp(Elapsed / FMath::Max(Profile->Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	if (Progress >= 1.f)
	{
		StopPop();
		return;
	}
	Widget->SetRenderScale(Profile->EvaluateScale(Progress));
}

void UWB_LabelPopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPop();
	Super::EndPlay(EndPlayReason);
}
