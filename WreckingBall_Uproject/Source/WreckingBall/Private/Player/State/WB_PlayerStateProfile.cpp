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

	// Transition table of the Phase C quote (2026-11-15). Hand rules are in StatesRequiringEmptyHands / StatesRequiringHolding.
	AllowedTransitions.Add(Player_State_Free, MakeContainer({
		Player_State_Dashing, Player_State_Kicking, Player_State_Throwing,
		Player_State_Stunned, Player_State_Down, Player_State_Disabled, Player_State_Celebrating }));

	const FGameplayTagContainer FromAction = MakeContainer({
		Player_State_Free, Player_State_Stunned, Player_State_Down, Player_State_Disabled });
	AllowedTransitions.Add(Player_State_Dashing, FromAction);
	AllowedTransitions.Add(Player_State_Kicking, FromAction);
	AllowedTransitions.Add(Player_State_Throwing, FromAction);

	AllowedTransitions.Add(Player_State_Stunned, MakeContainer({ Player_State_Free, Player_State_Down, Player_State_Disabled }));
	AllowedTransitions.Add(Player_State_Down, MakeContainer({ Player_State_Free, Player_State_Disabled }));
	AllowedTransitions.Add(Player_State_Disabled, MakeContainer({ Player_State_Free, Player_State_Celebrating }));
	AllowedTransitions.Add(Player_State_Celebrating, MakeContainer({ Player_State_Free, Player_State_Disabled }));

	StatesRequiringEmptyHands.AddTag(Player_State_Dashing);
	StatesRequiringHolding.AddTag(Player_State_Kicking);
	StatesRequiringHolding.AddTag(Player_State_Throwing);
}
