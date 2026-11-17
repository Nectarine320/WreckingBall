#include "Player/State/WB_PlayerStateTags.h"

namespace WBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State, "Player.State", "Root of what the player is doing. Exclusive: one state at a time, changed only by UWB_PlayerStateComponent::ChangePlayerState.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Free, "Player.State.Free", "Default state: the player moves and can start any action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Dashing, "Player.State.Dashing", "Dash in progress. Only allowed with empty hands.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Kicking, "Player.State.Kicking", "Kick in progress: the ball leaves at the Release anim notify (or the profile fallback duration).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Throwing, "Player.State.Throwing", "Throw of a held object in progress: the object leaves at the Release anim notify (or the profile fallback duration).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Stunned, "Player.State.Stunned", "Hit by a thrown object: slowed down, no action, no fall (step 10).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Down, "Player.State.Down", "Hit by a Dash: on the ground, no movement until the player gets up (step 10).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Recoiling, "Player.State.Recoiling", "Bounced back by a Dash into a surface too heavy to break (lot 10.3): short, no action, no movement, then Free.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Diving, "Player.State.Diving", "Defensive dive (UWB_PlayerDiveComponent, dive button pressed while moving): scripted jump along the stick, toward the ball when it is in the stick cone; landing and get-up, no control. Empty hands only.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Defending, "Player.State.Defending", "Defense stance (UWB_PlayerDiveComponent, dive button pressed without moving, kept while held): X pose, keeps the current facing, moves at reduced speed, blocks the ball. Empty hands only.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Disabled, "Player.State.Disabled", "No input has any effect (3-2-1, between rounds, end of match). Set and cleared by BP_SoccerGamemode only.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Celebrating, "Player.State.Celebrating", "The scoring team after a goal: runs at full speed, only the direction is controlled, no action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_State_Sulking, "Player.State.Sulking", "The team that conceded a goal: moves normally, no action (Dash, Grab, Kick, Throw), sulking emote.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Holding, "Player.Holding", "Root of what the player holds. Exclusive, changed only by UWB_PlayerStateComponent::ChangeHolding.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Holding_None, "Player.Holding.None", "Empty hands.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Holding_Ball, "Player.Holding.Ball", "The player holds the soccer ball.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Player_Holding_Object, "Player.Holding.Object", "The player holds a physics object (not the ball).");
}
