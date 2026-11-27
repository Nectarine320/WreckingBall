#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the gyms (devis gym-camera-p1, lot 2): who drives a place of the roster composition (FWB_RosterSlot::ControlType).
 * Player: a local player (gamepad) in ID order. Bot: a soccer bot (BP_SoccerAIController).
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gym_Control);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gym_Control_Player);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gym_Control_Bot);
}
