// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Gym/WB_GymSelectorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Regex.h"
#include "Misc/PackageName.h"
#include "Gym/WB_GymDefinition.h"
#include "Gym/WB_GymSettings.h"
#include "Match/Roster/WB_TeamSpawnPoint.h"
#include "ScopedTransaction.h"
#include "Tools/SortLevel/WB_SortLevelLibrary.h"
#include "Tools/WB_ToolsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_GymSelectorLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogWBGymSelector, Log, All);

#define LOCTEXT_NAMESPACE "WBGymSelector"

namespace WBGymSelector
{
	/** Editor world, null during Play In Editor (the map must not change while playing). */
	UWorld* GetEditorWorld(TArray<FString>& OutWarnings)
	{
		if (!GEditor)
		{
			return nullptr;
		}
		if (GEditor->PlayWorld)
		{
			OutWarnings.Add(TEXT("Stop Play In Editor to change the gym mode of the map."));
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	UClass* GetGymGameModeClass()
	{
		return GetDefault<UWB_ToolsDeveloperSettings>()->GymGameMode.LoadSynchronous();
	}

	bool IsGymGameMode(const UWorld* World)
	{
		const AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
		const UClass* GymClass = GetGymGameModeClass();
		return WorldSettings && GymClass && WorldSettings->DefaultGameMode && WorldSettings->DefaultGameMode->IsChildOf(GymClass);
	}

	/** First BP_GymSettings of the world (persistent level and loaded sub-levels). Warns when there are several. */
	AWB_GymSettings* FindGymSettings(UWorld* World, TArray<FString>& OutWarnings)
	{
		TArray<AWB_GymSettings*> Found;
		for (TActorIterator<AWB_GymSettings> It(World); It; ++It)
		{
			Found.Add(*It);
		}
		if (Found.Num() > 1)
		{
			OutWarnings.Add(FString::Printf(TEXT("%d BP_GymSettings in the map: %s is used, delete the others."), Found.Num(), *Found[0]->GetActorLabel()));
		}
		return Found.Num() > 0 ? Found[0] : nullptr;
	}

	/** Creates BP_GymSettings in the persistent level, in the folder that the Sort Level rules give it. */
	AWB_GymSettings* CreateGymSettings(UWorld* World, UClass* SettingsClass, TArray<FString>& OutWarnings)
	{
		FActorSpawnParameters Params;
		Params.OverrideLevel = World->PersistentLevel;
		AWB_GymSettings* Settings = World->SpawnActor<AWB_GymSettings>(SettingsClass, FTransform::Identity, Params);
		if (!Settings)
		{
			OutWarnings.Add(TEXT("BP_GymSettings could not be created in the persistent level."));
			return nullptr;
		}
		Settings->SetActorLabel(TEXT("GymSettings"));
		const FName Folder = UWB_SortLevelLibrary::FindFolderForClass(UWB_SortLevelLibrary::LoadRules(), SettingsClass);
		if (Folder.IsNone())
		{
			OutWarnings.Add(TEXT("No Sort Level rule for BP_GymSettings: it was created at the root of the Outliner."));
		}
		else
		{
			Settings->SetFolderPath(Folder);
		}
		UE_LOG(LogWBGymSelector, Log, TEXT("BP_GymSettings created in the persistent level of %s."), *World->GetOutermost()->GetName());
		return Settings;
	}
}

FWB_GymModeState UWB_GymSelectorLibrary::GetGymModeState()
{
	using namespace WBGymSelector;

	FWB_GymModeState State;
	UWorld* World = GetEditorWorld(State.Warnings);
	if (!World)
	{
		return State;
	}
	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
	if (!GetGymGameModeClass())
	{
		State.Warnings.Add(TEXT("Gym GameMode is not set in Project Settings > Editor > WB Tools."));
	}

	State.MapName = World->GetOutermost()->GetName();
	State.bGymModeEnabled = IsGymGameMode(World);
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	State.GameModeOverride = WorldSettings && WorldSettings->DefaultGameMode ? WorldSettings->DefaultGameMode->GetName() : TEXT("None");
	State.GymSettings = FindGymSettings(World, State.Warnings);
	State.ActiveGym = State.GymSettings ? State.GymSettings->GymDefinition : nullptr;
	State.bPlayer1Camera = GetDefault<UWB_ToolsUserSettings>()->bPIEStartWithPlayer1Camera;

	if (State.bGymModeEnabled)
	{
		FString GymFolder = Settings->GymMapsFolder.Path;
		if (!GymFolder.IsEmpty() && !GymFolder.EndsWith(TEXT("/")))
		{
			GymFolder += TEXT("/");
		}
		if (GymFolder.IsEmpty())
		{
			State.Warnings.Add(TEXT("Gym Maps Folder is not set in Project Settings > Editor > WB Tools."));
		}
		State.bShowCommitReminder = GymFolder.IsEmpty() || !State.MapName.StartsWith(GymFolder);
		if (!State.GymSettings)
		{
			State.Warnings.Add(TEXT("Gym mode is on but the map has no BP_GymSettings: choose a gym to create it."));
		}
		else if (!State.ActiveGym)
		{
			State.Warnings.Add(TEXT("No gym chosen: the gym director will keep the match rule (3 versus 3)."));
		}
		TActorIterator<AWB_TeamSpawnPoint> SpawnPoints(World);
		if (!SpawnPoints)
		{
			State.Warnings.Add(TEXT("The map has no BP_TeamSpawnPoint: no character can appear."));
		}
	}
	return State;
}

TArray<UWB_GymDefinition*> UWB_GymSelectorLibrary::GetGymDefinitions()
{
	TArray<FAssetData> Assets;
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetAssetsByClass(UWB_GymDefinition::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

	TArray<UWB_GymDefinition*> Gyms;
	for (const FAssetData& Asset : Assets)
	{
		if (UWB_GymDefinition* Gym = Cast<UWB_GymDefinition>(Asset.GetAsset()))
		{
			Gyms.Add(Gym);
		}
	}
	Gyms.Sort([](const UWB_GymDefinition& A, const UWB_GymDefinition& B)
	{
		const int32 Compare = A.GymName.ToString().Compare(B.GymName.ToString(), ESearchCase::IgnoreCase);
		return Compare != 0 ? Compare < 0 : A.GetName() < B.GetName();
	});
	return Gyms;
}

FWB_GymModeState UWB_GymSelectorLibrary::SetGymModeEnabled(bool bEnabled, UWB_GymDefinition* Gym)
{
	using namespace WBGymSelector;

	TArray<FString> Warnings;
	UWorld* World = GetEditorWorld(Warnings);
	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	if (!WorldSettings)
	{
		FWB_GymModeState State = GetGymModeState();
		State.Warnings.Append(Warnings);
		return State;
	}

	const bool bWasEnabled = IsGymGameMode(World);
	if (bEnabled)
	{
		UClass* GymClass = GetGymGameModeClass();
		UClass* SettingsClass = GetDefault<UWB_ToolsDeveloperSettings>()->GymSettingsClass.LoadSynchronous();
		if (!GymClass || !SettingsClass)
		{
			UE_LOG(LogWBGymSelector, Warning, TEXT("Gym GameMode or Gym Settings Class is not set in Project Settings > Editor > WB Tools: gym mode stays off."));
			FWB_GymModeState State = GetGymModeState();
			State.Warnings.Add(TEXT("Set Gym GameMode and Gym Settings Class in Project Settings > Editor > WB Tools."));
			return State;
		}

		const FScopedTransaction Transaction(LOCTEXT("GymModeOn", "Turn Gym Mode On"));
		AWB_GymSettings* GymSettings = FindGymSettings(World, Warnings);
		if (!GymSettings)
		{
			GymSettings = CreateGymSettings(World, SettingsClass, Warnings);
		}
		if (GymSettings)
		{
			GymSettings->Modify();
			if (!bWasEnabled)
			{
				GymSettings->bHasGameModeBeforeGym = true;
				GymSettings->GameModeBeforeGym = TSoftClassPtr<AGameModeBase>(WorldSettings->DefaultGameMode.Get());
			}
			if (Gym)
			{
				GymSettings->GymDefinition = Gym;
			}
		}
		if (!bWasEnabled)
		{
			WorldSettings->Modify();
			WorldSettings->DefaultGameMode = GymClass;
		}
		UE_LOG(LogWBGymSelector, Log, TEXT("Gym mode on in %s, gym %s."), *World->GetOutermost()->GetName(),
			GymSettings && GymSettings->GymDefinition ? *GymSettings->GymDefinition->GetName() : TEXT("none"));
	}
	else if (bWasEnabled)
	{
		const FScopedTransaction Transaction(LOCTEXT("GymModeOff", "Turn Gym Mode Off"));
		AWB_GymSettings* GymSettings = FindGymSettings(World, Warnings);
		UClass* Restored = nullptr;
		if (GymSettings && GymSettings->bHasGameModeBeforeGym)
		{
			Restored = GymSettings->GameModeBeforeGym.LoadSynchronous();
			GymSettings->Modify();
			GymSettings->bHasGameModeBeforeGym = false;
		}
		else
		{
			Warnings.Add(TEXT("The GameMode Override from before gym mode is unknown: it is set to None (project default GameMode)."));
		}
		WorldSettings->Modify();
		WorldSettings->DefaultGameMode = Restored;
		UE_LOG(LogWBGymSelector, Log, TEXT("Gym mode off in %s, GameMode Override back to %s."), *World->GetOutermost()->GetName(), Restored ? *Restored->GetName() : TEXT("None"));
	}

	FWB_GymModeState State = GetGymModeState();
	State.Warnings.Append(Warnings);
	return State;
}

FWB_GymModeState UWB_GymSelectorLibrary::SetActiveGym(UWB_GymDefinition* Gym)
{
	return SetGymModeEnabled(true, Gym);
}

FWB_GymModeState UWB_GymSelectorLibrary::SetPlayer1Camera(bool bPlayer1Camera)
{
	UWB_ToolsUserSettings* UserSettings = GetMutableDefault<UWB_ToolsUserSettings>();
	if (UserSettings->bPIEStartWithPlayer1Camera != bPlayer1Camera)
	{
		UserSettings->bPIEStartWithPlayer1Camera = bPlayer1Camera;
		UserSettings->SaveConfig();
		UE_LOG(LogWBGymSelector, Log, TEXT("Play In Editor starts with the %s camera."), bPlayer1Camera ? TEXT("Player 1") : TEXT("normal"));
	}
	ApplyPlayer1CameraPreference();
	return GetGymModeState();
}

void UWB_GymSelectorLibrary::ApplyPlayer1CameraPreference()
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("WB.StartWithPlayer1Camera"));
	if (!CVar)
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("Console variable WB.StartWithPlayer1Camera not found: the Camera Player 1 preference has no effect."));
		return;
	}
	CVar->Set(GetDefault<UWB_ToolsUserSettings>()->bPIEStartWithPlayer1Camera, ECVF_SetByCode);
}

TArray<FString> UWB_GymSelectorLibrary::GetLevelList()
{
	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
	if (Settings->LevelListInclude.IsEmpty())
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("Level List Include is not set in Project Settings > Editor > WB Tools: every map is listed."));
	}
	if (Settings->OfficialLevelPattern.IsEmpty() || Settings->GymLevelPattern.IsEmpty())
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("Official or Gym Level Pattern is not set in Project Settings > Editor > WB Tools: those maps are listed as Unsorted."));
	}

	TArray<FAssetData> Assets;
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/false);

	TArray<FString> Maps;
	for (const FAssetData& Asset : Assets)
	{
		const FString Name = Asset.AssetName.ToString();
		if (!Asset.PackagePath.ToString().StartsWith(TEXT("/Game"))
			|| (!Settings->LevelListInclude.IsEmpty() && !Name.Contains(Settings->LevelListInclude, ESearchCase::IgnoreCase))
			|| (!Settings->LevelListExclude.IsEmpty() && Name.Contains(Settings->LevelListExclude, ESearchCase::IgnoreCase)))
		{
			continue;
		}
		Maps.Add(Asset.PackageName.ToString());
	}
	Maps.Sort([](const FString& A, const FString& B)
	{
		const FString NameA = FPackageName::GetShortName(A);
		const FString NameB = FPackageName::GetShortName(B);
		const EWB_LevelCategory CategoryA = GetLevelCategoryOfPackage(A);
		const EWB_LevelCategory CategoryB = GetLevelCategoryOfPackage(B);
		return CategoryA != CategoryB ? CategoryA < CategoryB : NameA.Compare(NameB, ESearchCase::IgnoreCase) < 0;
	});
	return Maps;
}

EWB_LevelCategory UWB_GymSelectorLibrary::GetLevelCategoryOfPackage(const FString& MapPackage)
{
	const TSoftObjectPtr<UWorld>& Template = GetDefault<UWB_ToolsDeveloperSettings>()->LevelTemplate;
	if (!Template.IsNull())
	{
		const FString TemplateFolder = FPackageName::GetLongPackagePath(Template.ToSoftObjectPath().GetLongPackageName()) + TEXT("/");
		if (MapPackage.StartsWith(TemplateFolder))
		{
			return EWB_LevelCategory::Template;
		}
	}
	return GetLevelCategory(FPackageName::GetShortName(MapPackage));
}

EWB_LevelCategory UWB_GymSelectorLibrary::GetLevelCategory(const FString& MapName)
{
	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
	auto Matches = [&MapName](const FString& Pattern)
	{
		if (Pattern.IsEmpty())
		{
			return false;
		}
		FRegexMatcher Matcher(FRegexPattern(Pattern), MapName);
		return Matcher.FindNext();
	};
	if (Matches(Settings->OfficialLevelPattern))
	{
		return EWB_LevelCategory::Official;
	}
	if (Matches(Settings->GymLevelPattern))
	{
		return EWB_LevelCategory::Gym;
	}
	return EWB_LevelCategory::Unsorted;
}

TArray<FString> UWB_GymSelectorLibrary::GetSubLevels(const FString& MapPackage)
{
	const FString& Marker = GetDefault<UWB_ToolsDeveloperSettings>()->LevelListExclude;
	TArray<FName> Dependencies;
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetDependencies(FName(*MapPackage), Dependencies);

	TArray<FString> SubLevels;
	for (const FName& Dependency : Dependencies)
	{
		const FString Name = FPackageName::GetShortName(Dependency.ToString());
		if (!Marker.IsEmpty() && Name.Contains(Marker, ESearchCase::IgnoreCase))
		{
			SubLevels.AddUnique(Name);
		}
	}
	SubLevels.Sort();
	return SubLevels;
}

bool UWB_GymSelectorLibrary::BrowseToLevel(const FString& MapPackage)
{
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPackageName(FName(*MapPackage), Assets);
	if (Assets.Num() == 0 || !GEditor)
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("Map '%s' not found in the Asset Registry."), *MapPackage);
		return false;
	}
	GEditor->SyncBrowserToObjects(Assets);
	return true;
}

bool UWB_GymSelectorLibrary::OpenLevel(const FString& MapPackage)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("Stop Play In Editor before opening another map."));
		return false;
	}
	// Save / Don't Save / Cancel on every unsaved map and asset. False: the user cancelled.
	if (!FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/true, /*bSaveMapPackages=*/true, /*bSaveContentPackages=*/true))
	{
		return false;
	}
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(MapPackage, Filename, FPackageName::GetMapPackageExtension()))
	{
		UE_LOG(LogWBGymSelector, Warning, TEXT("'%s' is not a map package."), *MapPackage);
		return false;
	}
	return FEditorFileUtils::LoadMap(Filename, /*LoadAsTemplate=*/false, /*bShowProgress=*/true);
}

FString UWB_GymSelectorLibrary::StateToText(const FWB_GymModeState& State)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Map %s: gym mode %s, GameMode Override %s, BP_GymSettings %s, gym %s, Player 1 camera %s."),
		*State.MapName, State.bGymModeEnabled ? TEXT("ON") : TEXT("off"), *State.GameModeOverride,
		State.GymSettings ? *State.GymSettings->GetActorLabel() : TEXT("none"),
		State.ActiveGym ? *State.ActiveGym->GetName() : TEXT("none"), State.bPlayer1Camera ? TEXT("on") : TEXT("off")));
	if (State.bShowCommitReminder)
	{
		Lines.Add(TEXT("  REMINDER: this map is not a gym map. Turn gym mode off before committing it."));
	}
	for (const FString& Warning : State.Warnings)
	{
		Lines.Add(TEXT("  WARNING: ") + Warning);
	}
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
