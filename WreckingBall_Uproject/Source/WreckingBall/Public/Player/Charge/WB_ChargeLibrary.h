#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_ChargeLibrary.generated.h"

/** Hold-to-charge math shared by the charged actions (charged pass, charged Dash). */
UCLASS()
class WRECKINGBALL_API UWB_ChargeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Charge (0..1) of a button held HoldTime seconds: 0 under MinHoldTime (a tap), 1 at FullChargeTime, linear in between. */
	UFUNCTION(BlueprintPure, Category = "Charge")
	static float GetChargeFromHoldTime(float HoldTime, float MinHoldTime, float FullChargeTime);
};
