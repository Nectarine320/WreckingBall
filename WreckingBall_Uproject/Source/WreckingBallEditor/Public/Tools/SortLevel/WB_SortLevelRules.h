// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WB_SortLevelRules.generated.h"

/** Which Outliner folder a class of actor goes in. */
USTRUCT(BlueprintType)
struct FWB_SortClassRule
{
	GENERATED_BODY()

	/** Actors of this class and of its children follow the rule. When several rules match, the closest parent class wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	TSoftClassPtr<AActor> ActorClass;

	/** Outliner folder, e.g. "00_Layout/01_RoughBox". Its first part (00_Layout) picks the sub-level, see LevelRules. An actor already in a sub-folder of it stays there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	FName Folder;

	/** True: the actor goes in the persistent level instead of the sub-level of its folder (BP_GymSettings, read by GM_Gym). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	bool bStayInPersistentLevel = false;

	/** Why this rule exists, for the team. Not used by the tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level", meta = (MultiLine = true))
	FText Note;
};

/** Which sub-level a root Outliner folder goes in. */
USTRUCT(BlueprintType)
struct FWB_SortLevelRule
{
	GENERATED_BODY()

	/** Root Outliner folder, e.g. "00_Layout". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	FName RootFolder;

	/** End of the sub-level name, e.g. "_Layout" for SubLv_000_Soccerfield_Layout. The loaded sub-level whose name ends with it receives the actors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	FString SubLevelSuffix;
};

/**
 * Rules of the Sort Level tool (WB Tools > Level > Sort Level, devis wb-tools), set in Project Settings > WB Tools.
 * They write the Outliner and sub-level rule of CLAUDE.md ("Maps : Outliner et sous-niveaux").
 */
UCLASS(BlueprintType)
class WRECKINGBALLEDITOR_API UWB_SortLevelRules : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Class -> Outliner folder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level", meta = (TitleProperty = "{ActorClass} -> {Folder}"))
	TArray<FWB_SortClassRule> ClassRules;

	/** Root Outliner folder -> sub-level. A root folder with no rule here: the actor gets its folder but stays in its level, with a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level", meta = (TitleProperty = "{RootFolder} -> {SubLevelSuffix}"))
	TArray<FWB_SortLevelRule> LevelRules;

	/** Folder of the actors that no class rule matches. They stay in their level and are listed as a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	FName UnsortedFolder = TEXT("99_Unsorted");

	/** Classes Sort Level never touches (navigation data, world helpers...). Children of these classes are skipped too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sort Level")
	TArray<TSoftClassPtr<AActor>> IgnoredClasses;
};
