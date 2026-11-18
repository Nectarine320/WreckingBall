#include "Player/WB_PlayerProfileLibrary.h"

#include "GameFramework/Actor.h"
#include "Player/Command/WB_PlayerCommandComponent.h"
#include "Player/Dash/WB_PlayerDashProfile.h"
#include "Player/Energy/WB_PlayerEnergyProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"

const UWB_PlayerEnergyProfile* UWB_PlayerProfileLibrary::GetPlayerEnergyProfile(const AActor* Player)
{
	const UWB_PlayerStateComponent* State = Player ? Player->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	return State ? State->GetActiveEnergyProfile() : nullptr;
}

const UWB_PlayerDashProfile* UWB_PlayerProfileLibrary::GetPlayerDashProfile(const AActor* Player)
{
	const UWB_PlayerCommandComponent* Commands = Player ? Player->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
	return Commands ? Commands->GetDashProfile() : nullptr;
}
