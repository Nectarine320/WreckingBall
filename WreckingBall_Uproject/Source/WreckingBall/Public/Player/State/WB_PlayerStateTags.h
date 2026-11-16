#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the player state (Phase C, step 9).
 * Two exclusive families: Player.State.* (what the player does) and Player.Holding.* (what the player holds).
 * Declared in C++ so UWB_PlayerStateComponent can use Free / None as defaults; they show in the tag picker like ini tags.
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Free);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Dashing);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Kicking);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Throwing);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Stunned);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Down);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Disabled);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Celebrating);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_State_Sulking);

	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Holding);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Holding_None);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Holding_Ball);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Holding_Object);
}
