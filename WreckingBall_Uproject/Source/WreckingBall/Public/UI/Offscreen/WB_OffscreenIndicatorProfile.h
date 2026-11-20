#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_OffscreenIndicatorProfile.generated.h"

class UWB_OffscreenIndicatorItem_Base;

/**
 * Settings of the off-screen indicators (devis match 3v3, lot B): a disc at the team color at the screen edge, toward each character
 * that left the frame ("P1".."P4" on the humans, no text on the bots), smaller the farther the character is. Asset: DA_OffscreenIndicator_Default, set on WBP_OffscreenIndicators.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_OffscreenIndicatorProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** True: the indicators show at match start. Turn them on or off in game with SetIndicatorsEnabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
	bool bShowIndicators = true;

	/** Widget of one indicator (WBP_OffscreenIndicatorItem). Required: empty, no indicator is created and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
	TSubclassOf<UWB_OffscreenIndicatorItem_Base> ItemClass;

	/** True: the bots (COM) get an indicator too, a plain disc at the team color without text. False: only the humans (P1..P4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
	bool bShowBots = true;

	/** Size of an indicator, in UI units (pixels at 1080p), when its character is just past the screen edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Size", meta = (ClampMin = "8"))
	float SizeAtEdge = 56.f;

	/** Size of an indicator, in UI units, when its character is FarDistance or more past the screen edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Size", meta = (ClampMin = "4"))
	float SizeFar = 28.f;

	/** Distance past the screen edge, in UI units (as if the screen went on), at which the indicator reaches SizeFar. The size shrinks linearly in between. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Size", meta = (ClampMin = "1"))
	float FarDistance = 800.f;

	/** Width of the outline around the disc, in UI units. 0: no outline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Outline", meta = (ClampMin = "0"))
	float OutlineWidth = 3.f;

	/** Color of the outline around the disc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Outline")
	FLinearColor OutlineColor = FLinearColor::White;

	/** Gap between the screen edge and the indicator, in UI units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator", meta = (ClampMin = "0"))
	float EdgeMargin = 16.f;

	/** A character counts as off screen when its position is closer than this to the screen edge, in UI units (so a half-visible character already shows its indicator). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator", meta = (ClampMin = "0"))
	float OffscreenThreshold = 24.f;
};
