#include "Gym/WB_GymTags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gym_Control, "Gym.Control", "Who drives a place of a roster composition (gym DataAsset).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gym_Control_Player, "Gym.Control.Player", "The place is driven by a local player (gamepads in ID order). No player left: a bot takes it, with a warning.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gym_Control_Bot, "Gym.Control.Bot", "The place is driven by a soccer bot (BP_SoccerAIController).");
}
