#include "UI/Offscreen/WB_OffscreenIndicatorItem_Base.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UWB_OffscreenIndicatorItem_Base::NativeConstruct()
{
	Super::NativeConstruct();
	if (Disc)
	{
		// Round disc: a rounded box whose radius is half its height. The widget tint stays white so the outline keeps its own color.
		FSlateBrush Brush = Disc->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Disc->SetBrush(Brush);
		Disc->SetColorAndOpacity(FLinearColor::White);
	}
}

void UWB_OffscreenIndicatorItem_Base::SetIndicator(const FText& InLabel, FLinearColor InColor)
{
	if (Label && !InLabel.EqualTo(Label->GetText()))
	{
		Label->SetText(InLabel);
	}
	if (Disc && !InColor.Equals(CurrentColor))
	{
		CurrentColor = InColor;
		FSlateBrush Brush = Disc->GetBrush();
		Brush.TintColor = FSlateColor(InColor);
		Disc->SetBrush(Brush);
	}
}

void UWB_OffscreenIndicatorItem_Base::SetOutline(float Width, FLinearColor Color)
{
	if (!Disc)
	{
		return;
	}
	FSlateBrush Brush = Disc->GetBrush();
	Brush.OutlineSettings.Width = FMath::Max(Width, 0.f);
	Brush.OutlineSettings.Color = FSlateColor(Color);
	Disc->SetBrush(Brush);
}
