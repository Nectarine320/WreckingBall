#include "AI/Sight/WB_SightStimuliSourceComponent.h"

#include "Perception/AISense_Sight.h"

UWB_SightStimuliSourceComponent::UWB_SightStimuliSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegisterAsSource = true;
	RegisterAsSourceForSenses.AddUnique(UAISense_Sight::StaticClass());
}
