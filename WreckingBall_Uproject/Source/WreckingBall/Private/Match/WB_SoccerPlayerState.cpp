#include "Match/WB_SoccerPlayerState.h"

void AWB_SoccerPlayerState::SetTeam(int32 NewTeamIndex)
{
	NewTeamIndex = FMath::Max(NewTeamIndex, -1);
	if (NewTeamIndex == TeamIndex)
	{
		return;
	}
	TeamIndex = NewTeamIndex;
	OnTeamChanged.Broadcast(TeamIndex);
}

void AWB_SoccerPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// Seamless travel / reconnection: the new PlayerState keeps the team.
	if (AWB_SoccerPlayerState* Other = Cast<AWB_SoccerPlayerState>(PlayerState))
	{
		Other->SetTeam(TeamIndex);
	}
}
