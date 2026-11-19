#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "WB_DestructibleBlockersComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FWB_OnBlockerHitSignature, UPrimitiveComponent*, HitComponent, AActor*, OtherActor, UPrimitiveComponent*, OtherComp, FVector, NormalImpulse, const FHitResult&, Hit);

/**
 * Collects every primitive component of the owner that carries BlockerTag (the "blocker boxes" placed by the artist),
 * turns on their hit events and relays their hits through OnBlockerHit. A wall with an opening (window, door, arch)
 * simply has more tagged boxes: the owner's code does not change.
 * Hits from the owner itself and from actors attached to the owner (its dependents: door, glass, hung objects) are ignored,
 * so a door rubbing its frame never damages the wall.
 */
UCLASS(ClassGroup = (WreckingBall), Blueprintable, meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_DestructibleBlockersComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_DestructibleBlockersComponent();

	/** Component tag that marks a blocker box of the owner. Every box needs it (and the DestructibleBlocker collision preset). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destructible|Blockers")
	FName BlockerTag = TEXT("DestructibleBlocker");

	/** Called when a blocker box is hit by something that is neither the owner nor attached to it. Same parameters as OnComponentHit. */
	UPROPERTY(BlueprintAssignable, Category = "Destructible|Blockers")
	FWB_OnBlockerHitSignature OnBlockerHit;

	/**
	 * Finds the owner's primitive components tagged BlockerTag, turns on their hit events and listens to their hits.
	 * Called automatically at BeginPlay; call it again only if boxes are added at runtime. Returns the number of boxes found.
	 * Logs a warning when the owner has none (a prefab mounted without blocker box).
	 */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Blockers")
	int32 RefreshBlockers();

	/** Sets the collision of every blocker box (for example NoCollision when the wall is destroyed). */
	UFUNCTION(BlueprintCallable, Category = "Destructible|Blockers")
	void SetBlockersCollisionEnabled(ECollisionEnabled::Type NewType);

	/** Returns the blocker boxes found by the last RefreshBlockers. */
	UFUNCTION(BlueprintPure, Category = "Destructible|Blockers")
	TArray<UPrimitiveComponent*> GetBlockers() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleBlockerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Blockers;
};
