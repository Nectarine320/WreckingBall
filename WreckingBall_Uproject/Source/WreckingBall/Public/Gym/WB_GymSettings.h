#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WB_GymSettings.generated.h"

class UWB_GymDefinition;

/**
 * Chooses the gym of a map (devis gym-camera-p1, lot 2). Place one BP_GymSettings in the persistent level of a gym map:
 * UWB_GymDirectorComponent (GM_Gym) loads its GymDefinition when the match starts. Not visible in game.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_GymSettings : public AActor
{
	GENERATED_BODY()

public:
	AWB_GymSettings();

	/** Gym loaded when the map starts (DA_Gym_*). Empty: the gym director logs a warning and the roster keeps the match rule (3 versus 3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym")
	TObjectPtr<UWB_GymDefinition> GymDefinition;

	/** The BP_GymSettings of the world. None when there is none; a warning is logged when there are several (the first one is used). */
	UFUNCTION(BlueprintPure, Category = "Gym", meta = (WorldContext = "WorldContextObject"))
	static AWB_GymSettings* FindGymSettings(const UObject* WorldContextObject);
};
