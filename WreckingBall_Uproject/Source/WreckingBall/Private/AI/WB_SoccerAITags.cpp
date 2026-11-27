#include "AI/WB_SoccerAITags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole, "AI.ForcedRole", "Role a bot keeps whatever its team coordinator decides. Empty: the normal team roles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole_Goalkeeper, "AI.ForcedRole.Goalkeeper", "Permanent goalkeeper: always guards its goal (never moves up as the last man), claims the ball in its zone and clears it forward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole_Aggressive, "AI.ForcedRole.Aggressive", "Aggressive: goes for the free ball, presses the opponent carrier (Dash); with the ball it never shoots and keeps away from the nearest opponent.");
}
