#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the grab (lot 11.4): how a physics object is held, read from the GrabMode of its PDA_PhysicsProfile.
 * Centered: held in front of the player, rotation driven (the original grab). AtPoint: held by the contact point, free rotation (dragging heavy furniture).
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode_Centered);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode_AtPoint);
}
