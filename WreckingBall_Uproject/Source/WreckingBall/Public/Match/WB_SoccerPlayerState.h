#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "WB_SoccerPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnTeamChanged, int32, NewTeamIndex);

/**
 * PlayerState of a local player (UI 3D devis, lot A). Holds the team of the player: the single source of truth for the team.
 * Set as Player State Class of BP_SoccerGamemode; the GameMode writes the team (SetTeam) when the player is registered.
 * A bot that drives the player's pawn has no PlayerState: the pawn keeps a copy of the team in UWB_PlayerHitComponent::TeamIndex,
 * written by the character when OnTeamChanged fires. UWB_TeamLibrary::GetTeamOf reads the PlayerState first, then that copy.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_SoccerPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	/** Team of the player: 0 = red, 1 = blue (same values as UWB_TeamGoalComponent::DefendingTeamIndex). -1: no team yet. Change it with SetTeam. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Team")
	int32 TeamIndex = -1;

	/** Called after the team changed (not when SetTeam receives the current team). NewTeamIndex: -1 when the team was removed. */
	UPROPERTY(BlueprintAssignable, Category = "Team")
	FWB_OnTeamChanged OnTeamChanged;

	/** Sets the team of the player (0 = red, 1 = blue, -1 = no team) and fires OnTeamChanged when it changes. */
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetTeam(int32 NewTeamIndex);

	/** Team of the player: 0 = red, 1 = blue, -1 = no team. */
	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeam() const { return TeamIndex; }

	/** True when the player has a team (TeamIndex 0 or more). */
	UFUNCTION(BlueprintPure, Category = "Team")
	bool HasTeam() const { return TeamIndex >= 0; }

protected:
	virtual void CopyProperties(APlayerState* PlayerState) override;
};
