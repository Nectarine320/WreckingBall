#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the grab (lot 11.4): how a physics object is held, read from the GrabMode of its PDA_PhysicsProfile.
 * Centered: held in front of the player, rotation driven (the original grab). AtPoint: held by the contact point, free rotation (dragging heavy furniture).
 * Object.Size.* (devis gardien): size class of a physics object, read from the SizeTag of its PDA_PhysicsProfile (the Goalkeeper builds its shield with it).
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode_Centered);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Grab_Mode_AtPoint);

	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size_Tiny);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size_Small);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size_Medium);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size_Large);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Object_Size_XLarge);
}
