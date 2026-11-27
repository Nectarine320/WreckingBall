// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WB_GymSelectorLibrary.generated.h"

class AWB_GymSettings;

/** Group of a map in Gym Mode > Levels, from its name (patterns in Project Settings > Editor > WB Tools). */
UENUM(BlueprintType)
enum class EWB_LevelCategory : uint8
{
	/** Official level of the game (Lv_ + 3 digits + _). */
	Official,
	/** Gym map (Lv_ + 3 capital letters + _). */
	Gym,
	/** Any other map (art tests, old gyms...). */
	Unsorted
};
class UWB_GymDefinition;

/** Gym state of the map open in the editor, read from the map itself (never cached). */
USTRUCT(BlueprintType)
struct FWB_GymModeState
{
	GENERATED_BODY()

	/** Package of the map open in the editor. */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	FString MapName;

	/** True: the World Settings GameMode Override is the gym GameMode (GM_Gym or a child). */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	bool bGymModeEnabled = false;

	/** Name of the GameMode Override of the map ("None" = project default GameMode). */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	FString GameModeOverride;

	/** BP_GymSettings of the map, null when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	TObjectPtr<AWB_GymSettings> GymSettings;

	/** Gym chosen in BP_GymSettings, null when none. */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	TObjectPtr<UWB_GymDefinition> ActiveGym;

	/** True: Play In Editor starts with the Player 1 camera (personal preference, any map, gym mode or not). */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	bool bPlayer1Camera = false;

	/** True: gym mode is on in a map outside the gym maps folder. Do not commit the map like this. */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	bool bShowCommitReminder = false;

	/** Problems to show to the user (no team spawn point, several BP_GymSettings, no gym chosen...). */
	UPROPERTY(BlueprintReadOnly, Category = "Gym")
	TArray<FString> Warnings;
};

/**
 * Gym selector (WB Tools > Gym, devis wb-tools): turns any map into a gym. Turning gym mode on puts GM_Gym in the GameMode Override,
 * finds or creates BP_GymSettings in the persistent level and writes the chosen gym in it. Turning it off restores the GameMode Override
 * the map had before; BP_GymSettings stays in the map, inert, to keep the last gym.
 */
UCLASS()
class WRECKINGBALLEDITOR_API UWB_GymSelectorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the gym state of the map open in the editor. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static FWB_GymModeState GetGymModeState();

	/** Every gym definition of the project (found by class, wherever it is), sorted by GymName. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static TArray<UWB_GymDefinition*> GetGymDefinitions();

	/** Turns gym mode on or off in the map open in the editor. Gym: written in BP_GymSettings when turning on (None keeps the current one). */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static FWB_GymModeState SetGymModeEnabled(bool bEnabled, UWB_GymDefinition* Gym = nullptr);

	/** Chooses the gym of the map open in the editor. Turns gym mode on when it is off. The next Play In Editor uses it. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static FWB_GymModeState SetActiveGym(UWB_GymDefinition* Gym);

	/** Personal preference: every Play In Editor starts with the Player 1 camera (true) or with the normal start camera (false). Never saved in a map. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static FWB_GymModeState SetPlayer1Camera(bool bPlayer1Camera);

	/** Pushes the personal Player 1 camera preference to the console variable WB.StartWithPlayer1Camera (called when the editor starts). */
	static void ApplyPlayer1CameraPreference();

	/** Group of a map from its name: Official, Gym or Unsorted (patterns in Project Settings > Editor > WB Tools > Gym > Levels). */
	UFUNCTION(BlueprintPure, Category = "WB Tools|Gym")
	static EWB_LevelCategory GetLevelCategory(const FString& MapName);

	/** Maps of the project (/Game) whose name passes the Levels filters of Project Settings > Editor > WB Tools, sorted by group (Official, Gym, Unsorted), then by name. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static TArray<FString> GetLevelList();

	/** Sub-levels a map streams, read from the Asset Registry without loading the map (dependencies whose name contains Level List Exclude, e.g. SubLv), sorted. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static TArray<FString> GetSubLevels(const FString& MapPackage);

	/** Selects the map in the Content Browser (like Ctrl+B). Returns false when the map is not found. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static bool BrowseToLevel(const FString& MapPackage);

	/** Asks to save the unsaved changes (Save / Don't Save / Cancel), then opens the map. Returns false when cancelled or during Play In Editor. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Gym")
	static bool OpenLevel(const FString& MapPackage);

	/** State as plain text, for the log or the MCP agent. */
	static FString StateToText(const FWB_GymModeState& State);
};
