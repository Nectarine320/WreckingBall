#include "Player/Charge/WB_ChargeLibrary.h"

float UWB_ChargeLibrary::GetChargeFromHoldTime(float HoldTime, float MinHoldTime, float FullChargeTime)
{
	if (HoldTime < MinHoldTime)
	{
		return 0.f;
	}
	const float Range = FullChargeTime - MinHoldTime;
	// Range at or under zero: any hold past the minimum is a full charge.
	return Range > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((HoldTime - MinHoldTime) / Range, 0.f, 1.f) : 1.f;
}
