// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/WB_ToolsToolset.h"

#include "Gym/WB_GymDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_ToolsToolset)

FWB_SortLevelReport UWB_ToolsToolset::PreviewSortLevel()
{
	return UWB_SortLevelLibrary::PreviewSortLevel();
}

FWB_SortLevelReport UWB_ToolsToolset::ApplySortLevel()
{
	return UWB_SortLevelLibrary::ApplySortLevel();
}

FWB_GymModeState UWB_ToolsToolset::GetGymModeState()
{
	return UWB_GymSelectorLibrary::GetGymModeState();
}

TArray<UWB_GymDefinition*> UWB_ToolsToolset::GetGymDefinitions()
{
	return UWB_GymSelectorLibrary::GetGymDefinitions();
}

FWB_GymModeState UWB_ToolsToolset::SetGymModeEnabled(bool bEnabled, UWB_GymDefinition* Gym)
{
	return UWB_GymSelectorLibrary::SetGymModeEnabled(bEnabled, Gym);
}

FWB_GymModeState UWB_ToolsToolset::SetPlayer1Camera(bool bPlayer1Camera)
{
	return UWB_GymSelectorLibrary::SetPlayer1Camera(bPlayer1Camera);
}

TArray<FWB_DebugKeyEntry> UWB_ToolsToolset::GetDebugKeys()
{
	return UWB_DebugKeysLibrary::ScanDebugKeys();
}

bool UWB_ToolsToolset::SetDebugKeyDescription(const FString& Asset, FGuid NodeGuid, const FString& Description)
{
	return UWB_DebugKeysLibrary::SetDebugKeyDescription(Asset, NodeGuid, Description);
}
