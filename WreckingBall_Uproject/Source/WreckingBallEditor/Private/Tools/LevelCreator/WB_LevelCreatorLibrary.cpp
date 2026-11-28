// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/LevelCreator/WB_LevelCreatorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Gym/WB_GymDefinition.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "Tools/Gym/WB_GymSelectorLibrary.h"
#include "Tools/WB_ToolsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_LevelCreatorLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogWBLevelCreator, Log, All);

namespace WBLevelCreator
{
	IAssetRegistry& Registry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	FString FolderOf(const FDirectoryPath& Path)
	{
		FString Folder = Path.Path;
		Folder.RemoveFromEnd(TEXT("/"));
		return Folder;
	}

	/** Sub-levels the template streams (Asset Registry dependencies whose name contains the sub-level marker, e.g. SubLv). */
	TArray<FString> TemplateSubLevels(const FString& TemplatePackage)
	{
		const FString& Marker = GetDefault<UWB_ToolsDeveloperSettings>()->LevelListExclude;
		TArray<FName> Dependencies;
		Registry().GetDependencies(FName(*TemplatePackage), Dependencies);
		TArray<FString> SubLevels;
		for (const FName& Dependency : Dependencies)
		{
			const FString Package = Dependency.ToString();
			if (!Marker.IsEmpty() && FPackageName::GetShortName(Package).Contains(Marker, ESearchCase::IgnoreCase))
			{
				SubLevels.AddUnique(Package);
			}
		}
		SubLevels.Sort();
		return SubLevels;
	}

	/** True when a map with this exact name (case ignored) exists anywhere in /Game. */
	bool MapNameExists(const FString& MapName, FString& OutWhere)
	{
		TArray<FAssetData> Worlds;
		Registry().GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), Worlds);
		for (const FAssetData& World : Worlds)
		{
			if (World.AssetName.ToString().Equals(MapName, ESearchCase::IgnoreCase))
			{
				OutWhere = World.PackageName.ToString();
				return true;
			}
		}
		return false;
	}
}

FWB_NewLevelCheck UWB_LevelCreatorLibrary::CheckNewLevel(const FString& Code, const FString& Name)
{
	using namespace WBLevelCreator;

	FWB_NewLevelCheck Check;
	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();

	// ---------------------------------------------------------------- Code: exactly 3 digits or exactly 3 letters
	const FString CleanCode = Code.TrimStartAndEnd();
	int32 Digits = 0;
	int32 Letters = 0;
	for (const TCHAR Char : CleanCode)
	{
		Digits += FChar::IsDigit(Char) ? 1 : 0;
		Letters += (FChar::IsAlpha(Char) && Char < 128) ? 1 : 0;
	}
	if (CleanCode.Len() != 3)
	{
		Check.Error = TEXT("The code needs exactly 3 characters: 3 digits (official level) or 3 letters (gym).");
		return Check;
	}
	if (Digits > 0 && Letters > 0)
	{
		Check.Error = TEXT("The code mixes digits and letters: use 3 digits (official level) or 3 letters (gym), not both.");
		return Check;
	}
	if (Digits != 3 && Letters != 3)
	{
		Check.Error = TEXT("The code accepts only digits or letters (A to Z).");
		return Check;
	}
	Check.bGym = Letters == 3;
	const FString FinalCode = CleanCode.ToUpper();

	// ---------------------------------------------------------------- Name: letters and digits, starting with a letter
	const FString CleanName = Name.TrimStartAndEnd();
	if (CleanName.IsEmpty())
	{
		Check.Error = TEXT("Enter the level name, e.g. Bungalow or SoccerField.");
		return Check;
	}
	if (!FChar::IsAlpha(CleanName[0]))
	{
		Check.Error = TEXT("The level name must start with a letter.");
		return Check;
	}
	for (const TCHAR Char : CleanName)
	{
		if (!FChar::IsAlnum(Char) || Char >= 128)
		{
			Check.Error = TEXT("The level name accepts only letters and digits, without spaces or accents (e.g. SoccerField).");
			return Check;
		}
	}

	// ---------------------------------------------------------------- Settings and template
	const FString Folder = FolderOf(Check.bGym ? Settings->GymMapsFolder : Settings->GameMapsFolder);
	if (Folder.IsEmpty() || Settings->LevelNamePrefix.IsEmpty() || Settings->SubLevelsFolderName.IsEmpty() || Settings->LevelTemplate.IsNull())
	{
		Check.Error = TEXT("Set Level Template, Game Maps Folder, Gym Maps Folder, Level Name Prefix and Sub Levels Folder Name in Project Settings > Editor > WB Tools.");
		return Check;
	}
	const FString TemplatePackage = Settings->LevelTemplate.ToSoftObjectPath().GetLongPackageName();
	const FString TemplateName = FPackageName::GetShortName(TemplatePackage);
	FString TemplateToken = TemplateName;
	TemplateToken.RemoveFromStart(Settings->LevelNamePrefix);
	const TArray<FString> TemplateSubs = TemplateSubLevels(TemplatePackage);
	if (TemplateSubs.Num() == 0)
	{
		Check.Error = FString::Printf(TEXT("The level template %s has no sub-level (or is not saved)."), *TemplatePackage);
		return Check;
	}

	// ---------------------------------------------------------------- Names and existence
	const FString MapName = Settings->LevelNamePrefix + FinalCode + TEXT("_") + CleanName;
	const FString MapFolder = Folder / MapName;
	Check.MapPackage = MapFolder / MapName;
	FString Where;
	if (MapNameExists(MapName, Where) || FPackageName::DoesPackageExist(Check.MapPackage))
	{
		Check.Error = FString::Printf(TEXT("A map named %s already exists (%s): choose another code or name. Nothing is ever overwritten."), *MapName, Where.IsEmpty() ? *Check.MapPackage : *Where);
		Check.MapPackage.Reset();
		return Check;
	}
	for (const FString& TemplateSub : TemplateSubs)
	{
		const FString SubName = FPackageName::GetShortName(TemplateSub);
		if (!SubName.Contains(TemplateToken))
		{
			Check.Error = FString::Printf(TEXT("Template sub-level %s does not contain '%s': it cannot be renamed for the new level."), *SubName, *TemplateToken);
			return Check;
		}
		const FString NewSub = MapFolder / Settings->SubLevelsFolderName / SubName.Replace(*TemplateToken, *(FinalCode + TEXT("_") + CleanName));
		if (FPackageName::DoesPackageExist(NewSub))
		{
			Check.Error = FString::Printf(TEXT("%s already exists: nothing is ever overwritten."), *NewSub);
			return Check;
		}
		Check.SubLevelPackages.Add(NewSub);
	}
	Check.bValid = true;
	return Check;
}

FString UWB_LevelCreatorLibrary::CreateLevel(const FString& Code, const FString& Name, bool bPromptSave)
{
	using namespace WBLevelCreator;

	const FWB_NewLevelCheck Check = CheckNewLevel(Code, Name);
	if (!Check.bValid)
	{
		return TEXT("Not created: ") + Check.Error;
	}
	if (!GEditor || GEditor->PlayWorld)
	{
		return TEXT("Not created: stop Play In Editor first.");
	}
	if (bPromptSave && !FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/true, /*bSaveMapPackages=*/true, /*bSaveContentPackages=*/true))
	{
		return TEXT("Not created: cancelled.");
	}

	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
	const FString TemplatePackage = Settings->LevelTemplate.ToSoftObjectPath().GetLongPackageName();
	const TArray<FString> TemplateSubs = TemplateSubLevels(TemplatePackage);

	// A blank map first, so that no template level is open while it is duplicated.
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap=*/false);
	if (!World)
	{
		return TEXT("Not created: the blank map could not be made.");
	}

	TArray<FString> Lines;
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	for (int32 Index = 0; Index < TemplateSubs.Num() && Index < Check.SubLevelPackages.Num(); ++Index)
	{
		const FString& Source = TemplateSubs[Index];
		const FString& Target = Check.SubLevelPackages[Index];
		UWorld* SourceWorld = LoadObject<UWorld>(nullptr, *(Source + TEXT(".") + FPackageName::GetShortName(Source)));
		UObject* Copy = SourceWorld ? AssetTools.DuplicateAsset(FPackageName::GetShortName(Target), FPackageName::GetLongPackagePath(Target), SourceWorld) : nullptr;
		if (!Copy || !UEditorAssetLibrary::SaveLoadedAsset(Copy, /*bOnlyIfIsDirty=*/false))
		{
			return FString::Printf(TEXT("Not created: %s could not be duplicated to %s. Check the Output Log; delete the partial folder before trying again."), *Source, *Target);
		}
		if (!UEditorLevelUtils::AddLevelToWorld(World, *Target, ULevelStreamingAlwaysLoaded::StaticClass()))
		{
			return FString::Printf(TEXT("Not created: %s could not be streamed by the new level. Check the Output Log."), *Target);
		}
		Lines.Add(TEXT("  + ") + Target);
	}
	// The template streams a Nav Mesh Bounds Volume: build the navigation now, so the bots can move (it creates the RecastNavMesh).
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		// The editor locks navigation while a map loads (async load and initial locks): release them, then build at once.
		Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock | ENavigationBuildLock::InitialLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Navigation->Build();
		Lines.Add(TEXT("  Navigation built."));
	}
	else
	{
		Lines.Add(TEXT("  WARNING: no navigation system: build the paths (Build > Build Paths) before playing with bots."));
	}

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Check.MapPackage))
	{
		return FString::Printf(TEXT("Not created: %s could not be saved."), *Check.MapPackage);
	}

	if (Check.bGym)
	{
		const TArray<UWB_GymDefinition*> Gyms = UWB_GymSelectorLibrary::GetGymDefinitions();
		const FWB_GymModeState State = UWB_GymSelectorLibrary::SetGymModeEnabled(true, Gyms.Num() > 0 ? Gyms[0] : nullptr);
		UEditorLoadingAndSavingUtils::SaveMap(GEditor->GetEditorWorldContext().World(), Check.MapPackage);
		Lines.Add(FString::Printf(TEXT("  Gym mode on (gym %s): change it in WB Tools > Gym > Gym Mode."), State.ActiveGym ? *State.ActiveGym->GetName() : TEXT("none")));
	}

	const FString Report = FString::Printf(TEXT("Created %s%s, from %s:\n%s"), *Check.MapPackage, Check.bGym ? TEXT(" (gym)") : TEXT(""), *TemplatePackage, *FString::Join(Lines, TEXT("\n")));
	UE_LOG(LogWBLevelCreator, Log, TEXT("%s"), *Report);
	return Report;
}
