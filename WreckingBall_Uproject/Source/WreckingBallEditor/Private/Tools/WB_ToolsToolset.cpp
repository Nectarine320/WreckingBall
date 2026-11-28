// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/WB_ToolsToolset.h"

#include "Editor.h"
#include "Gym/WB_GymDefinition.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

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

FGameplayTagContainer UWB_ToolsToolset::GetCollectionTags(UObject* Asset)
{
	return UWB_CollectionsLibrary::GetCollectionTags(Asset);
}

bool UWB_ToolsToolset::SetCollectionTags(UObject* Asset, const FGameplayTagContainer& Tags)
{
	return UWB_CollectionsLibrary::SetCollectionTags(Asset, Tags);
}

FWB_CollectionsReport UWB_ToolsToolset::PreviewCollections()
{
	return UWB_CollectionsLibrary::PreviewCollections();
}

FWB_CollectionsReport UWB_ToolsToolset::UpdateCollections()
{
	return UWB_CollectionsLibrary::UpdateCollections();
}

FWB_NewLevelCheck UWB_ToolsToolset::CheckNewLevel(const FString& Code, const FString& Name)
{
	return UWB_LevelCreatorLibrary::CheckNewLevel(Code, Name);
}

FString UWB_ToolsToolset::CreateLevel(const FString& Code, const FString& Name)
{
	return UWB_LevelCreatorLibrary::CreateLevel(Code, Name, /*bPromptSave=*/false);
}

bool UWB_ToolsToolset::RunWBToolsScript(const FString& ScriptName)
{
	// A bare file name of the project's WB Tools scripts folder: no path, no other folder.
	if (ScriptName.IsEmpty() || ScriptName.Contains(TEXT("/")) || ScriptName.Contains(TEXT("\\")) || ScriptName.Contains(TEXT("..")) || !ScriptName.EndsWith(TEXT(".py")))
	{
		return false;
	}
	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Python/WBTools") / ScriptName);
	if (!GEditor || !IFileManager::Get().FileExists(*Script))
	{
		return false;
	}
	return GEditor->Exec(nullptr, *FString::Printf(TEXT("py \"%s\""), *Script));
}
