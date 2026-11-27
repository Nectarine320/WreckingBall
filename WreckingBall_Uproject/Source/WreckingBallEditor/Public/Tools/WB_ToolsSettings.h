// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "WB_ToolsSettings.generated.h"

class AGameModeBase;
class AWB_GymSettings;
class UWB_SortLevelRules;
class UWB_ToolsMenuData;

/**
 * Project Settings > Editor > WB Tools (devis wb-tools): the assets the WB Tools editor menu works with.
 * Editor only. The design team can change every entry without a programmer.
 */
UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "WB Tools"))
class WRECKINGBALLEDITOR_API UWB_ToolsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Extra entries of the WB Tools menu (DA_WBToolsMenu): Editor Utility Widgets and Python scripts added without recompiling. Empty: only the built-in tools are shown and a warning is logged. */
	UPROPERTY(Config, EditAnywhere, Category = "Menu")
	TSoftObjectPtr<UWB_ToolsMenuData> MenuData;

	/** Rules of the Sort Level tool (DA_SortLevelRules): which class goes in which Outliner folder, and which root folder goes in which sub-level. Empty: Sort Level does nothing and logs a warning. */
	UPROPERTY(Config, EditAnywhere, Category = "Sort Level")
	TSoftObjectPtr<UWB_SortLevelRules> SortLevelRules;

	/** GameMode put in the World Settings GameMode Override when gym mode is turned on in a map (GM_Gym). Empty: the gym panel refuses to turn gym mode on and logs a warning. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	TSoftClassPtr<AGameModeBase> GymGameMode;

	/** Gym settings actor created in the persistent level when a map has none (BP_GymSettings). Empty: the gym panel refuses to turn gym mode on and logs a warning. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	TSoftClassPtr<AWB_GymSettings> GymSettingsClass;

	/** Folder of the gym maps (/Game/Level/GYM). A map outside it shows a "do not commit in gym mode" reminder while gym mode is on. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym", meta = (ContentDir))
	FDirectoryPath GymMapsFolder;

	/** Gym Mode > Levels lists the maps whose name contains this text (ignoring case), e.g. "Lv" for Lv_000_Soccerfield and LV_AAA_Gym_SoccerField. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym|Levels")
	FString LevelListInclude;

	/** Gym Mode > Levels hides the maps whose name contains this text (ignoring case): "SubLv" hides the sub-levels. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym|Levels")
	FString LevelListExclude;

	/** Regular expression of an official level name, searched in the map name: "^[Ll][Vv]_[0-9][0-9][0-9]_" (Lv_ + 3 digits + _, e.g. Lv_000_Soccerfield). Shown first in Gym Mode > Levels. Do not use braces like {3}: the .ini file drops them. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym|Levels")
	FString OfficialLevelPattern;

	/** Regular expression of a gym level name, searched in the map name: "^[Ll][Vv]_[A-Z][A-Z][A-Z]_" (Lv_ + 3 capital letters + _, e.g. LV_AAA_Gym_SoccerField). Shown second; the other maps are Unsorted. Do not use braces like {3}: the .ini file drops them. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym|Levels")
	FString GymLevelPattern;

	/** Root of the collection tags (Collection). Its direct children are the master tags shown as checkboxes in WB Tools > Content > Collections. */
	UPROPERTY(Config, EditAnywhere, Category = "Collections")
	FGameplayTag CollectionRootTag;

	/** Master tag of the levels (Collection.Maps): its collection gets the sub-collections Official, Gyms and Unsorted from the level name patterns (Gym > Levels). */
	UPROPERTY(Config, EditAnywhere, Category = "Collections")
	FGameplayTag MapsCollectionTag;

	/** Collection of the Data Assets that have no collection tag at all: tag their class (C++ meta WBCollection, or the Collections button of the Blueprint editor). */
	UPROPERTY(Config, EditAnywhere, Category = "Collections")
	FString UntaggedCollectionName;

	/** Color of the collection of each master tag (Collection.Design, Collection.Environment...), shown in the Content Browser. A master tag without color: its collections have none. */
	UPROPERTY(Config, EditAnywhere, Category = "Collections", meta = (Categories = "Collection"))
	TMap<FGameplayTag, FLinearColor> MasterCollectionColors;

	/** How much darker each sub-collection is than its parent, on the color as seen on screen: 0.2 = 20% darker per level. */
	UPROPERTY(Config, EditAnywhere, Category = "Collections", meta = (ClampMin = "0", ClampMax = "1"))
	float CollectionShadeStep = 0.0f;

	virtual FName GetCategoryName() const override { return TEXT("Editor"); }
};

/**
 * Editor Preferences > WB Tools: personal settings of WB Tools, saved for this user only (never in a map, never committed).
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "WB Tools"))
class WRECKINGBALLEDITOR_API UWB_ToolsUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** True: every Play In Editor starts with the Player 1 camera, in any map and GameMode (console variable WB.StartWithPlayer1Camera). Set from WB Tools > Gym Mode. */
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	bool bPIEStartWithPlayer1Camera = false;

	/** Master tags (Collection.Design, Collection.Environment...) whose collections are built in your local collections. Unchecked ones are removed. Set from WB Tools > Content > Collections. */
	UPROPERTY(Config, EditAnywhere, Category = "Collections", meta = (Categories = "Collection"))
	FGameplayTagContainer EnabledMasterTags;

	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("General"); }
};
