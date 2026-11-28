#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the soccer bots (devis gym-camera-p1, lot 3): forced roles. A bot with a forced role keeps it whatever the
 * team coordinator decides (the coordinator leaves it out of its attribution). Set on a place of the roster composition (FWB_RosterSlot::ForcedRole).
 * AI.Behavior.* (devis gardien): what the bot brain is doing (its way of thinking), separate from Player.State.* (what its character does).
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole_Goalkeeper);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole_Aggressive);

	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Behavior);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Behavior_Goalkeeping);
}
