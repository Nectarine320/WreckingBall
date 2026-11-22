#include "UI/LabelPop/WB_LabelPopProfile.h"

namespace
{
	/** Adds smooth (cubic, auto tangent) keys to a curve. */
	void AddSmoothKeys(FRuntimeFloatCurve& Curve, std::initializer_list<TPair<float, float>> Keys)
	{
		FRichCurve* RichCurve = Curve.GetRichCurve();
		for (const TPair<float, float>& Key : Keys)
		{
			const FKeyHandle Handle = RichCurve->AddKey(Key.Key, Key.Value);
			RichCurve->SetKeyInterpMode(Handle, RCIM_Cubic);
			RichCurve->SetKeyTangentMode(Handle, RCTM_Auto);
		}
	}

	float EvaluateOrOne(const FRuntimeFloatCurve& Curve, float Progress)
	{
		const FRichCurve* RichCurve = Curve.GetRichCurveConst();
		return RichCurve && RichCurve->GetNumKeys() > 0 ? RichCurve->Eval(Progress) : 1.f;
	}
}

UWB_LabelPopProfile::UWB_LabelPopProfile()
{
	// Default squash and stretch: tall first, then wider, a bounce under 1, settle at 1. Shown and edited in the Data Asset.
	AddSmoothKeys(ScaleXCurve, { {0.f, 1.f}, {0.18f, 1.1f}, {0.38f, 1.28f}, {0.6f, 0.92f}, {0.8f, 1.04f}, {1.f, 1.f} });
	AddSmoothKeys(ScaleYCurve, { {0.f, 1.f}, {0.18f, 1.35f}, {0.38f, 1.15f}, {0.6f, 0.94f}, {0.8f, 1.03f}, {1.f, 1.f} });
}

FVector2D UWB_LabelPopProfile::EvaluateScale(float Progress) const
{
	return FVector2D(EvaluateOrOne(ScaleXCurve, Progress), EvaluateOrOne(ScaleYCurve, Progress));
}
