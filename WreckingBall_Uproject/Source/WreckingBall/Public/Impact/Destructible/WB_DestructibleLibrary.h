#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_DestructibleLibrary.generated.h"

class UMaterialInterface;
class UMeshComponent;

/**
 * Helpers shared by all destructibles (BP_Destructible_Base and its children).
 */
UCLASS()
class WRECKINGBALL_API UWB_DestructibleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the actors directly attached to Parent that implement WB_DestructibleDependent. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static void GetDependents(AActor* Parent, TArray<AActor*>& OutDependents);

	/** Calls OnParentDamaged on every dependent attached to Parent. Returns the number of dependents notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static int32 NotifyDependentsDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage);

	/** Calls OnParentDestroyed on every dependent attached to Parent. Call it once, when Parent enters the Destroyed state. Returns the number of dependents notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static int32 NotifyDependentsDestroyed(AActor* Parent);

	/**
	 * Returns which side of a wall a world-space normal points to: +1 = side A (actor +Y), -1 = side B (actor -Y), 0 = edge.
	 * A hit counts as a side hit when |dot(Normal, actor right)| is above MinSideDot (0.5 = same rule as the wall wobble).
	 */
	UFUNCTION(BlueprintPure, Category = "Destructible|Wall", meta = (DefaultToSelf = "Wall"))
	static int32 GetSideFromNormal(const AActor* Wall, FVector Normal, float MinSideDot = 0.5f);

	/** Applies Materials to Mesh: entry i goes to material slot i; an empty entry keeps the slot's current material. Extra entries are ignored. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Materials")
	static void ApplyMaterialOverrides(UMeshComponent* Mesh, const TArray<UMaterialInterface*>& Materials);
};
