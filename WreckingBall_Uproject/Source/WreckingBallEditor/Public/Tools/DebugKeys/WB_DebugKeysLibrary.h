// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WB_DebugKeysLibrary.generated.h"

/** One keyboard shortcut or console command of the project. */
USTRUCT(BlueprintType)
struct FWB_DebugKeyEntry
{
	GENERATED_BODY()

	/** True: a console command (WB.*). False: a keyboard event node of a Blueprint. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	bool bConsoleCommand = false;

	/** Key with its modifiers (e.g. "Ctrl+F7"), or the console command name. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString Key;

	/** Comment of the keyboard event node (editable from the Debug Keys panel), or the console help. Empty when the key is not documented. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString Description;

	/** Title of the Comment box around the node, shown as a hint only (often a block name, not a description of the key). */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString CommentBoxTitle;

	/** Guid of the keyboard event node in its Blueprint, used to edit its comment or open it. Invalid for a console command. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FGuid NodeGuid;

	/** Asset that holds the key (Blueprint package), empty for a console command. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString Asset;

	/** Graph of the Blueprint that holds the node (EventGraph...). */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString Graph;

	/** Other uses of the same key (another Blueprint node, or a game Input Mapping Context). Empty when the key is used once. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug Keys")
	FString Conflict;
};

/**
 * Debug keys reminder (WB Tools > Debug > Debug Keys, devis wb-tools): rebuilt from the project at each call, never kept by hand.
 * Scans the keyboard event nodes (Input Key, Input Debug Key) of the Blueprints in /Game, the keyboard keys of the Input Mapping
 * Contexts (for conflicts) and the WB.* console commands.
 */
UCLASS()
class WRECKINGBALLEDITOR_API UWB_DebugKeysLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Scans the project and returns every keyboard event node and WB.* console command, keys first, sorted by key. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Debug Keys")
	static TArray<FWB_DebugKeyEntry> ScanDebugKeys();

	/**
	 * Writes the description of a debug key in the comment of its node (shown as a pinned bubble in the graph). Save the Blueprint after.
	 * @param Asset Blueprint package, as given by ScanDebugKeys.
	 * @param NodeGuid Node, as given by ScanDebugKeys.
	 * @param Description One line that says what the key does. Empty removes the comment.
	 * @return True when the node was found and changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Debug Keys")
	static bool SetDebugKeyDescription(const FString& Asset, FGuid NodeGuid, const FString& Description);

	/** Opens the Blueprint that holds the key and centers its graph on the node. Returns false when the node is not found. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Debug Keys")
	static bool OpenDebugKey(const FString& Asset, FGuid NodeGuid);

	/** Selects the Blueprint that holds the key in the Content Browser (like Ctrl+B). Returns false when the asset is not found. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Debug Keys")
	static bool BrowseToDebugKey(const FString& Asset);

	/** The list as a Markdown table (Key | Description | Where), console commands in a second table. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Debug Keys")
	static FString ToMarkdown(const TArray<FWB_DebugKeyEntry>& Entries);
};
