#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_TeamRosterProfile.generated.h"

/**
 * Settings of the 3 versus 3 match roster (devis match 3v3, lot A): team size, bots at start, switch delay and the
 * fallback placement used when a team has no AWB_TeamSpawnPoint. Asset: DA_TeamRoster_Default, set on the
 * UWB_TeamRosterComponent of BP_SoccerGamemode.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_TeamRosterProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Characters per team at kickoff. Humans take the first places of their team, bots fill the rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roster", meta = (ClampMin = "1", ClampMax = "6"))
	int32 PlayersPerTeam = 3;

	/** True: the bots play as soon as the match starts. False (debug): the bot characters stand still until ToggleBots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roster")
	bool bStartBotsOnPlay = false;

	/** Minimum time in seconds between two character switches (or passes) of the same player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch", meta = (ClampMin = "0", Units = "Seconds"))
	float SwitchCooldown = 0.2f;

	/**
	 * True: when a bot teammate of a human takes the ball (loose ball after a lost dribble, rebound, interception...), that human takes the bot at once.
	 * With several humans in the team, the one who held the ball last in that team, else the nearest. False: the human keeps its character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	bool bTakeBotCarrier = false;

	/** Fallback placement (no AWB_TeamSpawnPoint for a team, or a missing place): distance in cm from the world origin to the characters. Keep it larger than the ball so nobody appears on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "50", Units = "Centimeters"))
	float FallbackRadius = 350.f;

	/** Fallback placement: distance in cm between two characters of the same team, along the circle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "50", Units = "Centimeters"))
	float FallbackSpacing = 180.f;
};
