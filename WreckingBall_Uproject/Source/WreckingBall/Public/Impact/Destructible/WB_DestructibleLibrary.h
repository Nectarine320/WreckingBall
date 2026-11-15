#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_DestructibleLibrary.generated.h"

class UGeometryCollectionComponent;
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

	/**
	 * Collapses a Geometry Collection: simulate, crumble the clusters, radial impulse from ImpulseOrigin (cm, constant falloff, velocity change).
	 * The debris then goes to the physics budget manager of the level when there is one; otherwise it is hidden and disabled after DebrisLifetime seconds (0 = never).
	 * Returns true when the debris was registered with a budget manager.
	 */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Collapse")
	static bool CollapseGeometryCollection(UGeometryCollectionComponent* Collection, FVector ImpulseOrigin, float ImpulseRadius = 600.f, float ImpulseStrength = 200.f, float DebrisLifetime = 10.f);
};
