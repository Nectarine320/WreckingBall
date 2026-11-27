#include "Camera/WB_CameraTags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Camera_Mode, "Camera.Mode", "What the shared camera follows.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Camera_Mode_Ball, "Camera.Mode.Ball", "Match camera: the ball, and the ball with its carrier when it is held (default).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Camera_Mode_Player1, "Camera.Mode.Player1", "Follows only the character of Player 1, with the same walk-direction offset as the carrier. Works without a ball.");
}
