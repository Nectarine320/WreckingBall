#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WB_OffscreenIndicatorItem_Base.generated.h"

class UImage;
class UTextBlock;

/**
 * One off-screen indicator: a round disc at the team color with the player label ("P1") on top. Child: WBP_OffscreenIndicatorItem,
 * which must hold an Image named Disc and a TextBlock named Label. The disc is drawn round by code (rounded box, half-height radius);
 * the team color fills it (brush tint) and an outline can surround it (SetOutline).
 */
UCLASS(Abstract, Blueprintable)
class WRECKINGBALL_API UWB_OffscreenIndicatorItem_Base : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the label text and the disc fill color (the outline keeps its own color). */
	UFUNCTION(BlueprintCallable, Category = "Indicator")
	void SetIndicator(const FText& InLabel, FLinearColor InColor);

	/** Sets the outline drawn around the disc. Width 0: no outline. */
	UFUNCTION(BlueprintCallable, Category = "Indicator")
	void SetOutline(float Width, FLinearColor Color);

protected:
	virtual void NativeConstruct() override;

	/** Round disc tinted with the team color. */
	UPROPERTY(BlueprintReadOnly, Category = "Indicator", meta = (BindWidget))
	TObjectPtr<UImage> Disc;

	/** Player label ("P1".."P4"). */
	UPROPERTY(BlueprintReadOnly, Category = "Indicator", meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

private:
	/** Last fill color set, so the brush is only rebuilt when it changes. */
	FLinearColor CurrentColor = FLinearColor::Transparent;
};
