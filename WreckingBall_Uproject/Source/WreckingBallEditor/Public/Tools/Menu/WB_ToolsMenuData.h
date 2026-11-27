// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

#include "WB_ToolsMenuData.generated.h"

class UEditorUtilityWidgetBlueprint;

/** What an extra WB Tools menu entry runs. */
UENUM(BlueprintType)
enum class EWB_ToolsMenuAction : uint8
{
	/** Opens an Editor Utility Widget in a tab. */
	EditorUtilityWidget,
	/** Runs a Python script file (needs the Python Editor Script Plugin). */
	PythonScript
};

/** One extra entry of the WB Tools menu. */
USTRUCT(BlueprintType)
struct FWB_ToolsMenuEntry
{
	GENERATED_BODY()

	/** Section of the menu: Level, Animation, Gym, Debug, or a new name (a new section is added at the end). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	FName Section = TEXT("Level");

	/** Text of the entry in the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	FText Label;

	/** Tooltip shown when the mouse is over the entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	FText ToolTip;

	/** What the entry runs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	EWB_ToolsMenuAction Action = EWB_ToolsMenuAction::EditorUtilityWidget;

	/** Editor Utility Widget opened by the entry (Action = EditorUtilityWidget). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (EditCondition = "Action == EWB_ToolsMenuAction::EditorUtilityWidget", EditConditionHides))
	TSoftObjectPtr<UEditorUtilityWidgetBlueprint> Widget;

	/** Python script run by the entry (Action = PythonScript), relative to the project folder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (EditCondition = "Action == EWB_ToolsMenuAction::PythonScript", EditConditionHides, FilePathFilter = "py", RelativeToGameDir))
	FFilePath PythonScript;
};

/**
 * Extra entries of the WB Tools editor menu (DA_WBToolsMenu, set in Project Settings > WB Tools). The built-in tools
 * (Sort Level, Gym, Debug Keys...) are always there; these entries are added after them, in their section.
 */
UCLASS(BlueprintType)
class WRECKINGBALLEDITOR_API UWB_ToolsMenuData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Entries added to the menu, in this order inside each section. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu", meta = (TitleProperty = "{Section} - {Label}"))
	TArray<FWB_ToolsMenuEntry> Entries;
};
