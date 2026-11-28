#include "Grab/WB_GrabTags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode, "Grab.Mode", "How a physics object is held (GrabMode of its PDA_PhysicsProfile).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode_Centered, "Grab.Mode.Centered", "Held in front of the player, centered, rotation driven by the player (original grab).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode_AtPoint, "Grab.Mode.AtPoint", "Held by the contact point, free rotation, the player slowed by the mass (heavy furniture is dragged).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size, "Object.Size", "Size class of a physics object (SizeTag of its PDA_PhysicsProfile). Used by the Goalkeeper to build its shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size_Tiny, "Object.Size.Tiny", "Tiny object (toast, book, photo frame): never part of a shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size_Small, "Object.Size.Small", "Small object (clock, cone, toaster): never part of a shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size_Medium, "Object.Size.Medium", "Medium object (chair, small table, tire): two of them make a shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size_Large, "Object.Size.Large", "Large object (armchair, kitchen table, cabinet): one of them makes a shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Object_Size_XLarge, "Object.Size.XLarge", "Extra large object (bed, dresser): one of them makes a shield.");
}
