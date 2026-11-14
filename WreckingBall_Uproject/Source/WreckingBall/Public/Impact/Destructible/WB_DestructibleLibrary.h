#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_DestructibleLibrary.generated.h"

/**
 * Helpers shared by all destructibles (BP_Destructible_Base and its children).
 */
UCLASS()
class WRECKINGBALL_API UWB_DestructibleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the actors directly attached to Parent that implement WB_DestructibleDependent. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents")
	static void GetDependents(AActor* Parent, TArray<AActor*>& OutDependents);

	/** Calls OnParentDamaged on every dependent attached to Parent. Returns the number of dependents notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents")
	static int32 NotifyDependentsDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage);

	/** Calls OnParentDestroyed on every dependent attached to Parent. Call it once, when Parent enters the Destroyed state. Returns the number of dependents notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents")
	static int32 NotifyDependentsDestroyed(AActor* Parent);
};
