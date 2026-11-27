// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WB_SortLevelLibrary.generated.h"

class AActor;
class UWB_SortLevelRules;

/** One actor that Sort Level moves (folder, level or both), or cannot sort. */
USTRUCT(BlueprintType)
struct FWB_SortLevelMove
{
	GENERATED_BODY()

	/** Label of the actor in the Outliner. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString ActorLabel;

	/** Class of the actor. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString ActorClass;

	/** Outliner folder before sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString FromFolder;

	/** Outliner folder after sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString ToFolder;

	/** Level before sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString FromLevel;

	/** Level after sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString ToLevel;

	/** True: no class rule matches, the actor goes in the unsorted folder. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	bool bUnsorted = false;

	/** Problem to check before applying (references that may break, missing sub-level...). Empty when none. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	FString Warning;
};

/** Result of a Sort Level preview or run. */
USTRUCT(BlueprintType)
struct FWB_SortLevelReport
{
	GENERATED_BODY()

	/** False: the map was only read (preview). True: the moves were applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	bool bApplied = false;

	/** Actors whose folder or level changes, and the unsorted ones. */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	TArray<FWB_SortLevelMove> Moves;

	/** Groups and attached actors left untouched, e.g. "Group_Sofa (5 actors)". */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	TArray<FString> SkippedGroups;

	/** Map-wide warnings (unloaded sub-levels, Level Sequences, missing rules...). */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	TArray<FString> Warnings;

	/** Number of actors already in place (not listed in Moves). */
	UPROPERTY(BlueprintReadOnly, Category = "Sort Level")
	int32 NumAlreadySorted = 0;
};

/**
 * Sort Level (WB Tools > Level, devis wb-tools): puts every actor of the map open in the editor in its Outliner folder and
 * sub-level, from the rules set in Project Settings > WB Tools. Grouped and attached actors are never moved, only listed.
 */
UCLASS()
class WRECKINGBALLEDITOR_API UWB_SortLevelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the map open in the editor and lists what Sort Level would move, without changing anything. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Sort Level")
	static FWB_SortLevelReport PreviewSortLevel();

	/** Sorts the map open in the editor (folders and sub-levels) and returns what was moved. Undo is available for the folders. Save the map after. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Sort Level")
	static FWB_SortLevelReport ApplySortLevel();

	/** Report as plain text, one line per move, for the log or the MCP agent. */
	static FString ReportToText(const FWB_SortLevelReport& Report);

	/** Folder that the rules give to this actor class (None when no rule matches). */
	static FName FindFolderForClass(const UWB_SortLevelRules* Rules, const UClass* ActorClass);

	/** Sort Level rules set in Project Settings > WB Tools. Logs a warning and returns null when the asset is not set. */
	static UWB_SortLevelRules* LoadRules();

private:
	static FWB_SortLevelReport Run(bool bApply);
};
