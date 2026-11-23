#include "Player/WB_PlayerProfileLibrary.h"

#include "GameFramework/Actor.h"
#include "Player/Command/WB_PlayerCommandComponent.h"
#include "Player/Dash/WB_PlayerDashProfile.h"
#include "Player/Energy/WB_PlayerEnergyProfile.h"
#include "Player/Kick/WB_KickProfile.h"
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

const UWB_KickProfile* UWB_PlayerProfileLibrary::GetPlayerKickProfile(const AActor* Player)
{
	const UWB_PlayerCommandComponent* Commands = Player ? Player->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
	const UWB_KickProfile* Profile = Commands ? Commands->KickProfile.Get() : nullptr;
	static bool bWarned = false;
	if (!Profile && Player && !bWarned)
	{
		bWarned = true;
		UE_LOG(LogTemp, Warning, TEXT("%s: no KickProfile on its UWB_PlayerCommandComponent (DA_Kick_Default expected). The kick charge does not rise."), *GetNameSafe(Player));
	}
	return Profile;
}
