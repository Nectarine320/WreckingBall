// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WB_CollectionsLibrary.generated.h"

/** One asset added to or removed from a collection. */
USTRUCT(BlueprintType)
struct FWB_CollectionChange
{
	GENERATED_BODY()

	/** Collection name. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	FString Collection;

	/** Asset path. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	FString Asset;

	/** True: added. False: removed (its tags no longer lead to this collection). */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	bool bAdded = true;
};

/** Result of a Collections preview or update. */
USTRUCT(BlueprintType)
struct FWB_CollectionsReport
{
	GENERATED_BODY()

	/** False: preview only. True: the local collections were changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	bool bApplied = false;

	/** Collections created (parent collections included). */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	TArray<FString> CreatedCollections;

	/** Collections removed because their master tag is unchecked. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	TArray<FString> RemovedCollections;

	/** Assets added to or removed from collections. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	TArray<FWB_CollectionChange> Changes;

	/** Classes whose Data Assets have no collection tag at all (they go in the Untagged collection), with their count. */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	TArray<FString> UntaggedClasses;

	/** Problems to check (unknown tags, settings not set...). */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	TArray<FString> Warnings;

	/** Number of collections built (created or kept). */
	UPROPERTY(BlueprintReadOnly, Category = "Collections")
	int32 NumCollections = 0;
};

/**
 * Collections (WB Tools > Content > Collections): builds the local Content Browser collections from Gameplay Tags Collection.*.
 * A tag sits on an asset (Blueprint or Data Asset, saved in its package), or on a C++ class (meta WBCollection). An asset gets, for each
 * master tag (Collection.Design, Collection.Environment...), the tags of the closest level that has one in that branch: the asset itself,
 * then its class, then its parent classes. Data Assets: every instance. Actor Blueprints: only the last child of each branch (no Blueprint
 * child, name not ending with _Base). Maps: from the level name patterns. Each tag gives one collection, its parent tag the parent collection.
 */
UCLASS()
class WRECKINGBALLEDITOR_API UWB_CollectionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Master tags: the direct children of the collection root tag (Project Settings > Editor > WB Tools > Collections). */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static TArray<FGameplayTag> GetMasterTags();

	/** True when this master tag is checked in your personal preferences (its collections are built). */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static bool IsMasterTagEnabled(FGameplayTag MasterTag);

	/** Checks or unchecks a master tag in your personal preferences. Run UpdateCollections after. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static void SetMasterTagEnabled(FGameplayTag MasterTag, bool bEnabled);

	/** Collection tags written on this asset itself (Blueprint or Data Asset). Inherited tags are not included. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static FGameplayTagContainer GetCollectionTags(UObject* Asset);

	/**
	 * Writes the collection tags of an asset (Blueprint or Data Asset), saved in its package. Empty removes them. Save the asset after.
	 * @return True when written.
	 */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static bool SetCollectionTags(UObject* Asset, const FGameplayTagContainer& Tags);

	/** Tags the asset inherits from its classes (closest per master tag), and where they come from. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static FGameplayTagContainer GetInheritedCollectionTags(UObject* Asset, FString& OutSources);

	/** Lists what UpdateCollections would change in your local collections, without changing them. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static FWB_CollectionsReport PreviewCollections();

	/** Builds your local collections from the tags: creates them, sets their parent, adds and removes assets, removes unchecked masters. */
	UFUNCTION(BlueprintCallable, Category = "WB Tools|Collections")
	static FWB_CollectionsReport UpdateCollections();

	/** Report as plain text, for the log or the MCP agent. */
	static FString ReportToText(const FWB_CollectionsReport& Report);

	/** Publishes the tags of saved assets in the Asset Registry (tag WBCollection), so the tool reads them without loading the assets. */
	static void RegisterAssetRegistryTags();
	static void UnregisterAssetRegistryTags();

private:
	static FWB_CollectionsReport Run(bool bApply);
};
