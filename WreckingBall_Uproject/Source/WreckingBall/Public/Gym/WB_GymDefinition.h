#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Match/Roster/WB_TeamRosterTypes.h"
#include "WB_GymDefinition.generated.h"

/**
 * One prototyping gym (devis gym-camera-p1, lot 2): the roster composition, the controls, the forced roles, and the match systems kept on or off.
 * Assets DA_Gym_* in /Game/Level/GYM/Data, read by UWB_GymDirectorComponent on GM_Gym. The active one is chosen in the map (BP_GymSettings),
 * or during the match with LoadGymDefinition (debug menu).
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_GymDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Name of the gym shown in the debug menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym")
	FText GymName;

	/** Every place of the gym, both teams. A team can have fewer than 3 places. Places Gym.Control.Player go to the local players in ID order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym|Composition", meta = (TitleProperty = "Team {Team} - {ControlType} - place {SpawnPlace}"))
	TArray<FWB_RosterSlot> Slots;

	/** True: the bots play as soon as the gym starts (observation gyms). False: they stand still until ToggleBots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym|Composition")
	bool bStartBotsOnPlay = false;

	/** True (default): the destructibles take damage and break as in the match. False: every impact on a destructible is refused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym|Systems")
	bool bDestructionEnabled = true;

	/** True: the match timer runs as in the match. False: it is frozen (the match never ends). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym|Systems")
	bool bTimerEnabled = false;

	/** Camera mode when the gym starts: Camera.Mode.Ball or Camera.Mode.Player1. Empty: the DefaultCameraMode of the GameMode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gym|Camera", meta = (Categories = "Camera.Mode"))
	FGameplayTag DefaultCameraMode;
};
