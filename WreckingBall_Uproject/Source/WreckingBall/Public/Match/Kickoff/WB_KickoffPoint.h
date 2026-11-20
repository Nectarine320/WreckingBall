#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WB_KickoffPoint.generated.h"

class UTextRenderComponent;

/**
 * Reference start point of a map (BP_KickoffPoint): the match ball appears here at kickoff and after each goal, and the shared
 * camera starts here. Drag one into each game map, on the floor where the ball should drop. The editor preview (KICKOFF label)
 * is hidden in game. Without one, the old behavior is kept (world origin) with a warning in the log.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_KickoffPoint : public AActor
{
	GENERATED_BODY()

public:
	AWB_KickoffPoint();

	/** Kickoff point of the world. None when there is none; a warning is logged when there are several (the first one is used). */
	UFUNCTION(BlueprintPure, Category = "Kickoff", meta = (WorldContext = "WorldContextObject"))
	static AWB_KickoffPoint* FindKickoffPoint(const UObject* WorldContextObject);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	/** Editor preview label, lying flat and readable from the top camera. Hidden in game. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> PreviewLabel;
};
