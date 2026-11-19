#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Impact/Destructible/WB_DestructibleDependent.h"
#include "WB_DestructibleDependentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FWB_OnParentDamagedSignature, AActor*, Parent, FVector, ImpactLocation, FVector, ImpactNormal, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnParentDestroyedSignature, AActor*, Parent);

/**
 * Makes its owner a dependent of the destructible it is attached to, without adding the WB_DestructibleDependent
 * interface to the Blueprint by hand: the destructible calls this component, which relays the calls as events.
 * Add it to a dependent Blueprint (glass, poster...) and bind ParentDamaged / ParentDestroyed in its Event Graph.
 */
UCLASS(ClassGroup = (WreckingBall), Blueprintable, meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_DestructibleDependentComponent : public UActorComponent, public IWB_DestructibleDependent
{
	GENERATED_BODY()

public:
	UWB_DestructibleDependentComponent();

	/** Called when the destructible the owner is attached to took damage (location and normal of its impact, damage it received). */
	UPROPERTY(BlueprintAssignable, Category = "Destructible|Dependent")
	FWB_OnParentDamagedSignature ParentDamaged;

	/** Called once when the destructible the owner is attached to enters the Destroyed state. */
	UPROPERTY(BlueprintAssignable, Category = "Destructible|Dependent")
	FWB_OnParentDestroyedSignature ParentDestroyed;

	virtual void OnParentDamaged_Implementation(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage) override;
	virtual void OnParentDestroyed_Implementation(AActor* Parent) override;
};
