#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Chaos/ChaosEngineInterface.h"
#include "WB_DestructibleLibrary.generated.h"

class UGeometryCollectionComponent;
class UMaterialInterface;
class UMeshComponent;
class UPhysicalMaterial;

/**
 * Helpers shared by all destructibles (BP_Destructible_Base and its children).
 */
UCLASS()
class WRECKINGBALL_API UWB_DestructibleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the actors directly attached to Parent that implement WB_DestructibleDependent, themselves or through one of their components (UWB_DestructibleDependentComponent, UWB_HingedDoorComponent). */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static void GetDependents(AActor* Parent, TArray<AActor*>& OutDependents);

	/** Calls OnParentDamaged on every dependent attached to Parent (the actor and each of its components that implement the interface). Returns the number of objects notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static int32 NotifyDependentsDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage);

	/** Calls OnParentDestroyed on every dependent attached to Parent (the actor and each of its components that implement the interface). Call it once, when Parent enters the Destroyed state. Returns the number of objects notified. */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Dependents", meta = (DefaultToSelf = "Parent"))
	static int32 NotifyDependentsDestroyed(AActor* Parent);

	/**
	 * Returns which side of a wall a world-space normal points to: +1 = side A (actor +Y), -1 = side B (actor -Y), 0 = edge.
	 * A hit counts as a side hit when |dot(Normal, actor right)| is above MinSideDot (0.5 = same rule as the wall wobble).
	 */
	UFUNCTION(BlueprintPure, Category = "Destructible|Wall", meta = (DefaultToSelf = "Wall"))
	static int32 GetSideFromNormal(const AActor* Wall, FVector Normal, float MinSideDot = 0.5f);

	/**
	 * Returns Normal, flipped if needed so that it points from the hit surface toward Source (outward, toward the attacker).
	 * Physics hit events (OnComponentHit) can report the contact normal in either direction, depending on the order of the two bodies;
	 * sweep hits (Dash scan) already point outward. Use it before building impact data from a physics hit.
	 * Source empty: Normal is returned unchanged.
	 */
	UFUNCTION(BlueprintPure, Category = "Destructible|Impact")
	static FVector OrientNormalTowardSource(FVector Normal, FVector ImpactPoint, const AActor* Source);

	/**
	 * Returns the surface type of the wall side a hit lands on, for impact feedback: SideA's surface for side A (actor +Y), SideB's for side B (actor -Y).
	 * Normal is the world-space impact normal, pointing toward the attacker (same side rule as GetSideFromNormal, MinSideDot 0.5).
	 * Returns Fallback for an edge hit or when the hit side has no physical material.
	 */
	UFUNCTION(BlueprintPure, Category = "Destructible|Wall", meta = (DefaultToSelf = "Wall"))
	static TEnumAsByte<EPhysicalSurface> GetWallSideSurface(const AActor* Wall, FVector Normal, const UPhysicalMaterial* SideA, const UPhysicalMaterial* SideB, TEnumAsByte<EPhysicalSurface> Fallback);

	/**
	 * Per-instance material list of a mesh, for the Construction Script: resizes Overrides to the number of material slots of Mesh (no more, no less),
	 * fills every empty entry with the material the slot currently shows (so the Details panel lists the materials in use), then applies the list to Mesh.
	 * To go back to the mesh material after a change, clear the entry: it is filled again on the next construction.
	 */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Materials")
	static void SyncMaterialOverrides(UMeshComponent* Mesh, UPARAM(ref) TArray<UMaterialInterface*>& Overrides);

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
