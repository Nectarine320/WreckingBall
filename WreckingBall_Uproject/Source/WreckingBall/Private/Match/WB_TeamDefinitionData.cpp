#include "Match/WB_TeamDefinitionData.h"

bool UWB_TeamDefinitionData::FindTeamDefinition(int32 TeamIndex, FWB_TeamDefinition& OutDefinition) const
{
	if (Teams.IsValidIndex(TeamIndex))
	{
		OutDefinition = Teams[TeamIndex];
		return true;
	}
	OutDefinition = NoTeam;
	return false;
}
