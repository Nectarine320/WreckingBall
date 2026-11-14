#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WB_DestructibleDependent.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWB_DestructibleDependent : public UInterface
{
	GENERATED_BODY()
};

/**
 * An actor that depends on a destructible it is attached to (glass, door, hung frame, clock...).
 * The destructible tells its attached dependents what happens to it; each dependent picks its own reaction.
 * If the parent is destroyed, the dependent should go too. The reverse is false.
 * Replaces BPI_DestructibleDependent from the lot 9.5 devis (a C++ interface needs no manual asset creation).
 */
class WRECKINGBALL_API IWB_DestructibleDependent
{
	GENERATED_BODY()

public:
	/** Called when the parent destructible took damage. Location and Normal come from the parent's impact data; Damage is the damage the parent received. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Destructible|Dependent")
	void OnParentDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage);

	/** Called once when the parent destructible enters the Destroyed state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Destructible|Dependent")
	void OnParentDestroyed(AActor* Parent);
};
