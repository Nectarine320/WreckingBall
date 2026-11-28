// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WB_LevelCreatorLibrary.generated.h"

/** What Create Level would make from a code and a name, or why it refuses. */
USTRUCT(BlueprintType)
struct FWB_NewLevelCheck
{
	GENERATED_BODY()

	/** True: the level can be created. */
	UPROPERTY(BlueprintReadOnly, Category = "Create Level")
	bool bValid = false;

	/** Why the level cannot be created (empty when valid). */
	UPROPERTY(BlueprintReadOnly, Category = "Create Level")
	FString Error;

	/** True: code of 3 letters, the level is a gym (gym folder, gym mode on). */
	UPROPERTY(BlueprintReadOnly, Category = "Create Level")
	bool bGym = false;

	/** Package of the new persistent level, e.g. /Game/Level/Game/Lv_200_School/Lv_200_School. */
	UPROPERTY(BlueprintReadOnly, Category = "Create Level")
	FString MapPackage;

	/** Packages of the new sub-levels, duplicated from the template. */
	UPROPERTY(BlueprintReadOnly, Category = "Create Level")
	TArray<FString> SubLevelPackages;
};

/**
 * Create Level (WB Tools > Level > Create Level): makes a level from a 3-character code and a name. 3 digits = official level in the game maps
 * folder, 3 letters = gym in the gym maps folder (gym mode on). The sub-levels of the template level (Lv_Template) are duplicated and streamed
 * by the new persistent level. Never overwrites: a map with the same name anywhere in the project is refused.
 */
UCLASS()
class WRECKINGBALLEDITOR_API UWB_LevelCreatorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Checks a code (3 digits or 3 letters) and a name (letters and digits) and lists what would be created. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Create Level")
	static FWB_NewLevelCheck CheckNewLevel(const FString& Code, const FString& Name);

	/**
	 * Creates the level and opens it. Asks to save the unsaved changes first (Cancel stops).
	 * @param Code 3 digits (official level) or 3 letters (gym).
	 * @param Name Level name, letters and digits, e.g. Bungalow or SoccerField.
	 * @param bPromptSave True: ask to save the unsaved changes first (editor panel). False: the MCP agent saved them already.
	 * @return Report of the creation, or why it failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Create Level")
	static FString CreateLevel(const FString& Code, const FString& Name, bool bPromptSave = true);
};
