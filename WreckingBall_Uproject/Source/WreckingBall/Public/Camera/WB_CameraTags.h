#pragma once

#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags of the shared camera modes (devis gym-camera-p1, lot 4), read by UWB_CameraModeComponent.
 * Ball: frames the ball and its carrier (match camera). Player1: follows only the character of Player 1.
 */
namespace WBTags
{
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_Mode);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_Mode_Ball);
	WRECKINGBALL_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_Mode_Player1);
}
