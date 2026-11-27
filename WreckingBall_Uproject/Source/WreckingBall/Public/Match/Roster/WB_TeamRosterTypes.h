#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WB_TeamRosterTypes.generated.h"

/**
 * One place of a roster composition (devis gym-camera-p1, lot 2): replaces the "PlayersPerTeam on each team" rule of UWB_TeamRosterComponent
 * when a composition is set (SetSlotComposition, used by the gyms). A team can have fewer places than the other.
 */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_RosterSlot
{
	GENERATED_BODY()

	/** Team of the place: 0 = red, 1 = blue. Its look (colors, material) comes from DA_TeamDefinitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RosterSlot", meta = (ClampMin = "0", ClampMax = "1"))
	int32 Team = 0;

	/** Who drives the place: Gym.Control.Player (local players in ID order; none left: a bot, with a warning) or Gym.Control.Bot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RosterSlot", meta = (Categories = "Gym.Control"))
	FGameplayTag ControlType;

	/** Role kept by the bot of this place whatever the team coordinator decides (AI.ForcedRole.*). Empty: the normal team roles. Ignored while a human drives it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RosterSlot", meta = (Categories = "AI.ForcedRole"))
	FGameplayTag ForcedRole;

	/** Place of the team's BP_TeamSpawnPoint where the character appears at kickoff and after each goal: 1 to 3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RosterSlot", meta = (ClampMin = "1", ClampMax = "3"))
	int32 SpawnPlace = 1;
};
