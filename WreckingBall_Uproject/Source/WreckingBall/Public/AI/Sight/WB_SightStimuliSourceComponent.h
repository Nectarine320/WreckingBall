#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "WB_SightStimuliSourceComponent.generated.h"

/**
 * Makes its actor visible to the bots' sight (AIPerception, sense Sight), devis gardien. Added to the physics objects (BP_M_PhysicObject),
 * the destructibles (BP_Destructible_Base) and the players (BP_GenericCharacter). Registered for Sight by default, so the actors already
 * placed in the levels get it too. The Goalkeeper finds its shield objects with it; walls and objects still block the view.
 */
UCLASS(ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_SightStimuliSourceComponent : public UAIPerceptionStimuliSourceComponent
{
	GENERATED_BODY()

public:
	UWB_SightStimuliSourceComponent(const FObjectInitializer& ObjectInitializer);
};
