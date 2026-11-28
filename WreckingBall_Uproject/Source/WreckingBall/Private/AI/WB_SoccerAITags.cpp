#include "AI/WB_SoccerAITags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole, "AI.ForcedRole", "Role a bot keeps whatever its team coordinator decides. Empty: the normal team roles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole_Goalkeeper, "AI.ForcedRole.Goalkeeper", "Permanent goalkeeper: the bot always plays the Goalkeeper behavior (AI.Behavior.Goalkeeping), whatever its team coordinator decides.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_ForcedRole_Aggressive, "AI.ForcedRole.Aggressive", "Aggressive: goes for the free ball, presses the opponent carrier (Dash); with the ball it never shoots and keeps away from the nearest opponent.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_Behavior, "AI.Behavior", "What a bot brain is doing (its way of thinking). Not the state of its character (Player.State.*).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_Behavior_Goalkeeping, "AI.Behavior.Goalkeeping", "Goalkeeper behavior (devis gardien): guards its goal against threats, builds a shield of objects when there is none. Its character still takes the defense stance, dives or kicks.");
}
