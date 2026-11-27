#include "Gym/WB_GymDirectorComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Gym/WB_GymDefinition.h"
#include "Gym/WB_GymSettings.h"
#include "Gym/WB_GymTags.h"
#include "Impact/Destructible/WB_DestructibleLibrary.h"
#include "Match/Roster/WB_TeamRosterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBGym, Log, All);

UWB_GymDirectorComponent::UWB_GymDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWB_GymDirectorComponent* UWB_GymDirectorComponent::GetGymDirector(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	return GameMode ? GameMode->FindComponentByClass<UWB_GymDirectorComponent>() : nullptr;
}

void UWB_GymDirectorComponent::BeginPlay()
{
	// Runs before the BeginPlay of the GameMode Blueprint, which builds the roster: the composition is in place by then.
	Super::BeginPlay();

	const AWB_GymSettings* Settings = AWB_GymSettings::FindGymSettings(this);
	if (!Settings || !Settings->GymDefinition)
	{
		UE_LOG(LogWBGym, Warning, TEXT("%s: no BP_GymSettings with a GymDefinition in the map. The roster keeps the match rule (3 versus 3), destruction and timer unchanged."),
			*GetPathNameSafe(this));
		return;
	}
	LoadGymDefinition(Settings->GymDefinition);
}

UWB_TeamRosterComponent* UWB_GymDirectorComponent::GetRoster() const
{
	UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this);
	if (!Roster)
	{
		UE_LOG(LogWBGym, Warning, TEXT("%s: the GameMode has no UWB_TeamRosterComponent, the gym composition cannot be applied."), *GetPathNameSafe(this));
	}
	return Roster;
}

bool UWB_GymDirectorComponent::LoadGymDefinition(UWB_GymDefinition* GymDefinition)
{
	if (!GymDefinition)
	{
		UE_LOG(LogWBGym, Warning, TEXT("LoadGymDefinition: no gym given, nothing changed."));
		return false;
	}
	ActiveGym = GymDefinition;
	Slots = GymDefinition->Slots;
	UWB_DestructibleLibrary::SetDestructionEnabled(this, GymDefinition->bDestructionEnabled);
	const bool bApplied = ApplySlots();
	UE_LOG(LogWBGym, Log, TEXT("Gym %s loaded: %d places, destruction %s, timer %s."), *GymDefinition->GetName(), Slots.Num(),
		GymDefinition->bDestructionEnabled ? TEXT("on") : TEXT("off"), GymDefinition->bTimerEnabled ? TEXT("on") : TEXT("off"));
	OnGymLoaded.Broadcast(GymDefinition);
	return bApplied;
}

bool UWB_GymDirectorComponent::SetSlotControl(int32 SlotIndex, FGameplayTag ControlTag)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogWBGym, Warning, TEXT("SetSlotControl: no place %d in the active gym."), SlotIndex);
		return false;
	}
	if (!ControlTag.MatchesTagExact(WBTags::Gym_Control_Player) && !ControlTag.MatchesTagExact(WBTags::Gym_Control_Bot))
	{
		UE_LOG(LogWBGym, Warning, TEXT("SetSlotControl: %s is not Gym.Control.Player nor Gym.Control.Bot, nothing changed."), *ControlTag.ToString());
		return false;
	}
	Slots[SlotIndex].ControlType = ControlTag;
	const bool bApplied = ApplySlots();
	OnGymLoaded.Broadcast(ActiveGym);
	return bApplied;
}

bool UWB_GymDirectorComponent::SetSlotRole(int32 SlotIndex, FGameplayTag RoleTag)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogWBGym, Warning, TEXT("SetSlotRole: no place %d in the active gym."), SlotIndex);
		return false;
	}
	Slots[SlotIndex].ForcedRole = RoleTag;
	UWB_TeamRosterComponent* Roster = GetRoster();
	return Roster && Roster->SetSlotForcedRole(SlotIndex, RoleTag);
}

bool UWB_GymDirectorComponent::ApplySlots()
{
	UWB_TeamRosterComponent* Roster = GetRoster();
	if (!Roster || !ActiveGym)
	{
		return false;
	}
	Roster->SetSlotComposition(Slots, ActiveGym->bStartBotsOnPlay);
	// Before the GameMode Blueprint built it, BuildRoster will use the composition; afterwards, rebuild now.
	return Roster->IsRosterBuilt() ? Roster->RebuildRoster() : true;
}

bool UWB_GymDirectorComponent::IsTimerEnabled() const
{
	// No active gym: the timer runs as in the match.
	return !ActiveGym || ActiveGym->bTimerEnabled;
}

FGameplayTag UWB_GymDirectorComponent::GetDefaultCameraMode() const
{
	return ActiveGym ? ActiveGym->DefaultCameraMode : FGameplayTag();
}
