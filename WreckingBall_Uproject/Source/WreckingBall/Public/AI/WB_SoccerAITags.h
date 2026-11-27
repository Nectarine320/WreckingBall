#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the soccer bots (devis gym-camera-p1, lot 3): forced roles. A bot with a forced role keeps it whatever the
 * team coordinator decides (the coordinator leaves it out of its attribution). Set on a place of the roster composition (FWB_RosterSlot::ForcedRole).
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole_Goalkeeper);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_ForcedRole_Aggressive);
}
