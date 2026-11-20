#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "WB_GoalkeeperZoneComponent.generated.h"

/**
 * Goalkeeper zone of a goal (devis NPC, phase F): a box around the goal mouth, also the "net perimeter" the bots measure from.
 * Added to BP_GoalPost_01 next to its UWB_TeamGoalComponent (the team comes from it). No collision: the bots test it by math.
 * A bot whose role is Goalkeeper enters the defense stance while it stands inside its own zone.
 * The tests are flat (the box height is ignored): only the X / Y extent of the box counts.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_GoalkeeperZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UWB_GoalkeeperZoneComponent();

	/** Team that defends this zone: the DefendingTeamIndex of the UWB_TeamGoalComponent of the same actor. -1 when the actor has none. */
	UFUNCTION(BlueprintPure, Category = "GoalkeeperZone")
	int32 GetDefendingTeamIndex() const;

	/** True when Location is inside the box (flat test), grown by Margin cm on each side (negative: shrunk). */
	UFUNCTION(BlueprintPure, Category = "GoalkeeperZone")
	bool IsLocationInside(FVector Location, float Margin = 0.f) const;

	/** Flat distance in cm from Origin (inside the box) to the box border along Direction. 0 when Origin is outside or Direction is zero. */
	UFUNCTION(BlueprintPure, Category = "GoalkeeperZone")
	float GetExitDistance(FVector Origin, FVector Direction) const;

	/** The goalkeeper zone TeamIndex defends. None when the world has none. */
	UFUNCTION(BlueprintPure, Category = "GoalkeeperZone", meta = (WorldContext = "WorldContextObject"))
	static UWB_GoalkeeperZoneComponent* FindZoneForTeam(const UObject* WorldContextObject, int32 TeamIndex);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	/** Zones of every world (filtered by world on query). */
	static TArray<TWeakObjectPtr<UWB_GoalkeeperZoneComponent>> Registry;
};
