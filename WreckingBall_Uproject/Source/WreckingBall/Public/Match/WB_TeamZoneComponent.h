#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "WB_TeamZoneComponent.generated.h"

/**
 * Defensive zone of a team (devis gardien): the part of the play area that belongs to TeamIndex (its half of the field, around its goal).
 * Placed in the map on BP_TeamZone (folder 01_Gameplay/04_PlayArea of the Gameplay sub-level), one per team. No collision: the bots test it by math.
 * The Goalkeeper bot reads it for its "Threat" condition: an opponent carrier, or an opponent and the ball, inside the zone.
 * The tests are flat (the box height is ignored): only the X / Y extent of the box counts.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_TeamZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UWB_TeamZoneComponent();

	/** Team this zone belongs to: 0 = red, 1 = blue (same values as UWB_TeamGoalComponent::DefendingTeamIndex). -1: no team (ignored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamZone")
	int32 TeamIndex = 0;

	/** True when Location is inside the box (flat test), grown by Margin cm on each side (negative: shrunk). */
	UFUNCTION(BlueprintPure, Category = "TeamZone")
	bool IsLocationInside(FVector Location, float Margin = 0.f) const;

	/** The zone of InTeamIndex. None when the world has none. */
	UFUNCTION(BlueprintPure, Category = "TeamZone", meta = (WorldContext = "WorldContextObject"))
	static UWB_TeamZoneComponent* FindZoneForTeam(const UObject* WorldContextObject, int32 InTeamIndex);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	/** Zones of every world (filtered by world on query). */
	static TArray<TWeakObjectPtr<UWB_TeamZoneComponent>> Registry;
};
