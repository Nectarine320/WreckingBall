#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Match/WB_TeamDefinitionData.h"
#include "WB_TeamLibrary.generated.h"

class AWB_SoccerPlayerState;

/**
 * Team helpers shared by every system (UI 3D devis, lot A): HUD, ground ring, overhead label, GameMode, bots.
 * The team lives in AWB_SoccerPlayerState. A pawn driven by a bot has no PlayerState: its team is then read from the copy kept by
 * its UWB_PlayerHitComponent (TeamIndex).
 */
UCLASS()
class WRECKINGBALL_API UWB_TeamLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Team of Actor: 0 = red, 1 = blue, -1 = no team or invalid Actor.
	 * Actor may be a pawn (its AWB_SoccerPlayerState, else the TeamIndex of its UWB_PlayerHitComponent), a controller or a PlayerState.
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static int32 GetTeamOf(const AActor* Actor);

	/** True when A and B both have a team and it is the same team. False when one of them has no team or A is B. */
	UFUNCTION(BlueprintPure, Category = "Team")
	static bool AreTeammates(const AActor* A, const AActor* B);

	/** AWB_SoccerPlayerState of Actor (a pawn, a controller or the PlayerState itself). None for a pawn driven by a bot. */
	UFUNCTION(BlueprintPure, Category = "Team")
	static AWB_SoccerPlayerState* GetSoccerPlayerState(const AActor* Actor);

	/** Team definitions set in Project Settings > Game > Wrecking Ball Teams. None, with a warning in the log, when the setting is empty. */
	UFUNCTION(BlueprintPure, Category = "Team")
	static const UWB_TeamDefinitionData* GetTeamDefinitions();

	/**
	 * Look of TeamIndex (label and ring colors, body material) from the project team definitions.
	 * Returns false when the definitions are missing (warning in the log; OutDefinition is then a default white entry) or TeamIndex has no entry (OutDefinition is NoTeam).
	 */
	UFUNCTION(BlueprintPure, Category = "Team", meta = (ReturnDisplayName = "Found"))
	static bool GetTeamDefinition(int32 TeamIndex, FWB_TeamDefinition& OutDefinition);
};
