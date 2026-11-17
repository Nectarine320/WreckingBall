#include "Player/State/WB_PlayerStateProfile.h"

#include "Player/State/WB_PlayerStateTags.h"

UWB_PlayerStateProfile::UWB_PlayerStateProfile()
{
	using namespace WBTags;

	auto MakeContainer = [](std::initializer_list<FGameplayTag> Tags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : Tags)
		{
			Container.AddTag(Tag);
		}
		return Container;
	};

	// Transition table of the Phase C quote (updated 2026-11-15 with Sulking). Hand rules are in StatesRequiringEmptyHands / StatesRequiringHolding.
	AllowedTransitions.Add(Player_State_Free, MakeContainer({
		Player_State_Dashing, Player_State_Kicking, Player_State_Throwing, Player_State_Stunned,
		Player_State_Down, Player_State_Disabled, Player_State_Celebrating, Player_State_Sulking, Player_State_Diving, Player_State_Defending }));

	// An action in progress is cancelled by a goal (Celebrating / Sulking).
	const FGameplayTagContainer FromAction = MakeContainer({
		Player_State_Free, Player_State_Stunned, Player_State_Down, Player_State_Disabled,
		Player_State_Celebrating, Player_State_Sulking });
	// Dashing can also end in a bounce off a surface too heavy to break (lot 10.3).
	FGameplayTagContainer FromDashing = FromAction;
	FromDashing.AddTag(Player_State_Recoiling);
	AllowedTransitions.Add(Player_State_Dashing, FromDashing);
	AllowedTransitions.Add(Player_State_Kicking, FromAction);
	AllowedTransitions.Add(Player_State_Throwing, FromAction);

	// Stunned / Down: the scoring team goes to Celebrating; the other team keeps its state until the end of the celebration.
	// Recoiling: same exits as an action (a hit, a goal or the GameMode cut the bounce short).
	AllowedTransitions.Add(Player_State_Recoiling, FromAction);
	// Defensive dive: same exits as an action (a hit or a goal cuts it short; the dive component goes back to Free after the get-up).
	AllowedTransitions.Add(Player_State_Diving, FromAction);
	// Defense stance: same exits (released button: Free; Dash hit: Down / Stunned; goal; Disabled).
	AllowedTransitions.Add(Player_State_Defending, FromAction);

	AllowedTransitions.Add(Player_State_Stunned, MakeContainer({ Player_State_Free, Player_State_Down, Player_State_Disabled, Player_State_Celebrating }));
	AllowedTransitions.Add(Player_State_Down, MakeContainer({ Player_State_Free, Player_State_Disabled, Player_State_Celebrating }));
	AllowedTransitions.Add(Player_State_Disabled, MakeContainer({ Player_State_Free, Player_State_Celebrating, Player_State_Sulking }));
	AllowedTransitions.Add(Player_State_Celebrating, MakeContainer({ Player_State_Free, Player_State_Disabled }));
	AllowedTransitions.Add(Player_State_Sulking, MakeContainer({ Player_State_Free, Player_State_Disabled }));

	StatesRequiringEmptyHands.AddTag(Player_State_Dashing);
	StatesRequiringEmptyHands.AddTag(Player_State_Diving);
	StatesRequiringEmptyHands.AddTag(Player_State_Defending);
	StatesRequiringHolding.AddTag(Player_State_Kicking);
	StatesRequiringHolding.AddTag(Player_State_Throwing);

	// Stunned blocks movement too (user decision 2026-11-16): a stunned player sits on the ground.
	// Recoiling and Diving block both too: the bounce and the dive are scripted.
	StatesBlockingMovement = MakeContainer({ Player_State_Disabled, Player_State_Down, Player_State_Stunned, Player_State_Recoiling, Player_State_Diving });
	StatesBlockingSprint = MakeContainer({ Player_State_Disabled, Player_State_Down, Player_State_Stunned, Player_State_Celebrating, Player_State_Recoiling, Player_State_Diving, Player_State_Defending });
}
