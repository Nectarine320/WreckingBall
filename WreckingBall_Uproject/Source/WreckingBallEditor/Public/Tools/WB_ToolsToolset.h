// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/Collections/WB_CollectionsLibrary.h"
#include "Tools/DebugKeys/WB_DebugKeysLibrary.h"
#include "Tools/Gym/WB_GymSelectorLibrary.h"
#include "Tools/LevelCreator/WB_LevelCreatorLibrary.h"
#include "Tools/SortLevel/WB_SortLevelLibrary.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "WB_ToolsToolset.generated.h"

class UWB_GymDefinition;

/**
 * WB Tools for the MCP agent (devis wb-tools): the same tools as the WB Tools editor menu. Sort Level after placing actors,
 * the gym mode of the open map, and the list of debug keys. They all work on the map open in the editor.
 */
UCLASS(BlueprintType, Hidden)
class UWB_ToolsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lists what Sort Level would change in the open map (Outliner folder and sub-level of each actor), without changing anything.
	 * Rules come from Project Settings > Editor > WB Tools. Grouped and attached actors are listed in SkippedGroups and never moved.
	 */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Sort Level")
	static FWB_SortLevelReport PreviewSortLevel();

	/** Sorts the open map: sets the Outliner folders and moves actors to their sub-level. Run PreviewSortLevel first, save the map after. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Sort Level")
	static FWB_SortLevelReport ApplySortLevel();

	/** Gym state of the open map: gym mode on or off, BP_GymSettings, chosen gym, warnings. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Gym")
	static FWB_GymModeState GetGymModeState();

	/** Every gym definition (DA_Gym_*) of the project, sorted by GymName. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Gym")
	static TArray<UWB_GymDefinition*> GetGymDefinitions();

	/**
	 * Turns gym mode on or off in the open map.
	 * @param bEnabled On: GameMode Override = gym GameMode, BP_GymSettings found or created. Off: the previous GameMode Override comes back.
	 * @param Gym Gym written in BP_GymSettings when turning on. None keeps the current one.
	 */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Gym")
	static FWB_GymModeState SetGymModeEnabled(bool bEnabled, UWB_GymDefinition* Gym);

	/** Personal preference of this editor: every Play In Editor starts with the Player 1 camera (true) or the normal start camera (false). Never saved in a map. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Gym")
	static FWB_GymModeState SetPlayer1Camera(bool bPlayer1Camera);

	/** Scans the project for keyboard event nodes of Blueprints and WB.* console commands, with descriptions and conflicts. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Debug Keys")
	static TArray<FWB_DebugKeyEntry> GetDebugKeys();

	/**
	 * Writes the description of a debug key in the comment of its Blueprint node. Save the Blueprint after.
	 * @param Asset Blueprint package of the entry (GetDebugKeys).
	 * @param NodeGuid Node of the entry (GetDebugKeys).
	 * @param Description One line that says what the key does.
	 */
	/** Checks a level code (3 digits = official, 3 letters = gym) and name, and lists the packages Create Level would make. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Create Level")
	static FWB_NewLevelCheck CheckNewLevel(const FString& Code, const FString& Name);

	/** Creates a level from the template and opens it (never overwrites). Save the open map before calling: no save prompt here. Returns the report. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Create Level")
	static FString CreateLevel(const FString& Code, const FString& Name);

	/**
	 * Runs one of the project's WB Tools Python scripts in the editor (Content/Python/WBTools/<ScriptName>), for the steps that need the
	 * open editor (moving actors between levels...). Only the files of that folder can run.
	 * @param ScriptName File name in Content/Python/WBTools, e.g. build_level_template.py.
	 * @return True when the script was found and started (its log lines are in the Output Log).
	 */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Scripts")
	static bool RunWBToolsScript(const FString& ScriptName);

	/** Collection tags written on an asset itself (Blueprint or Data Asset), without the inherited ones. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Collections")
	static FGameplayTagContainer GetCollectionTags(UObject* Asset);

	/**
	 * Writes the collection tags (Collection.*) of a Blueprint or Data Asset; several allowed, empty removes them. Save the asset after.
	 * Tag every new Data Asset class (PDA) this way, or with meta WBCollection on a C++ class, then run UpdateCollections.
	 */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Collections")
	static bool SetCollectionTags(UObject* Asset, const FGameplayTagContainer& Tags);

	/** Lists what UpdateCollections would change in the local collections of this editor. */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Collections")
	static FWB_CollectionsReport PreviewCollections();

	/** Builds the local collections of this editor from the collection tags (checked master tags only). */
	UFUNCTION(meta = (AICallable), Category = "WB Tools|Collections")
	static FWB_CollectionsReport UpdateCollections();

	UFUNCTION(meta = (AICallable), Category = "WB Tools|Debug Keys")
	static bool SetDebugKeyDescription(const FString& Asset, FGuid NodeGuid, const FString& Description);
};
