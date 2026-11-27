#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WB_GymSettings.generated.h"

class AGameModeBase;
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

#if WITH_EDITORONLY_DATA
	/** Editor only, written by WB Tools > Gym: true when GameModeBeforeGym holds the map's GameMode Override from before gym mode was turned on. */
	UPROPERTY(VisibleAnywhere, Category = "Gym|Editor")
	bool bHasGameModeBeforeGym = false;

	/** Editor only, written by WB Tools > Gym: the map's GameMode Override before gym mode was turned on, restored when it is turned off. None: the project default GameMode. */
	UPROPERTY(VisibleAnywhere, Category = "Gym|Editor")
	TSoftClassPtr<AGameModeBase> GameModeBeforeGym;
#endif

	/** The BP_GymSettings of the world. None when there is none; a warning is logged when there are several (the first one is used). */
	UFUNCTION(BlueprintPure, Category = "Gym", meta = (WorldContext = "WorldContextObject"))
	static AWB_GymSettings* FindGymSettings(const UObject* WorldContextObject);
};
