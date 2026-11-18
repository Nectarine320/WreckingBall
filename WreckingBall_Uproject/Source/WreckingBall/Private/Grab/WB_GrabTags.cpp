#include "Grab/WB_GrabTags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode, "Grab.Mode", "How a physics object is held (GrabMode of its PDA_PhysicsProfile).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode_Centered, "Grab.Mode.Centered", "Held in front of the player, centered, rotation driven by the player (original grab).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Grab_Mode_AtPoint, "Grab.Mode.AtPoint", "Held by the contact point, free rotation, the player slowed by the mass (heavy furniture is dragged).");
}
