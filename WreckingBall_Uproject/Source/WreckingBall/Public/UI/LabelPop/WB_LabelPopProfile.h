#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "WB_LabelPopProfile.generated.h"

/**
 * Settings of the label pop (DA_LabelPop_Default): the squash and stretch played on the P1-P4 label above a character
 * each time a player takes that character. Read by UWB_LabelPopComponent.
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.UI"))
class WRECKINGBALL_API UWB_LabelPopProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWB_LabelPopProfile();

	/** Length of the pop, in seconds. The curves are read over this time (X = 0 at the start, X = 1 at the end). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LabelPop", meta = (ClampMin = "0.01", Units = "Seconds"))
	float Duration = 0.5f;

	/**
	 * Horizontal scale of the label during the pop: X = progress (0..1 of Duration), Y = render scale (1 = default size).
	 * Default: grows a little wider after the vertical stretch, bounces under 1, then settles. End the curve at 1 so the label ends at its default size.
	 * An empty curve keeps the width at 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LabelPop")
	FRuntimeFloatCurve ScaleXCurve;

	/**
	 * Vertical scale of the label during the pop: X = progress (0..1 of Duration), Y = render scale (1 = default size).
	 * Default: stretches up first, then scales down, bounces under 1, then settles. End the curve at 1 so the label ends at its default size.
	 * An empty curve keeps the height at 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LabelPop")
	FRuntimeFloatCurve ScaleYCurve;

	/** Point of the label that stays in place while it scales, in label space: (0.5, 0.5) = center, (0.5, 1) = bottom center (the label grows upward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LabelPop")
	FVector2D RenderPivot = FVector2D(0.5f, 1.f);

	/** Render scale at Progress (0..1 of Duration), read from ScaleXCurve and ScaleYCurve. An empty curve gives 1 on its axis. */
	FVector2D EvaluateScale(float Progress) const;
};
