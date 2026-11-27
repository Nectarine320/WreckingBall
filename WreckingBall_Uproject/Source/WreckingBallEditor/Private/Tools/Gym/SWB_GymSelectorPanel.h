// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/Gym/WB_GymSelectorLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STextBlock;
class UWB_GymDefinition;

/** One gym of the list. */
struct FWB_GymListItem
{
	TWeakObjectPtr<UWB_GymDefinition> Gym;
	FText Label;
};

/** One row of Gym Mode > Levels: a group header (Official, Gym, Unsorted) or a map. */
struct FWB_LevelListItem
{
	/** Map package. Empty for a group header. */
	FString MapPackage;
	/** Text of a group header. */
	FText Header;
	/** Sub-levels of the map, short names joined (e.g. "Gameplay, Layout, Light"). */
	FString SubLevels;
	EWB_LevelCategory Category = EWB_LevelCategory::Unsorted;

	bool IsHeader() const { return MapPackage.IsEmpty(); }
};

/**
 * WB Tools > Gym Mode: turns the open map into a gym and chooses the gym (always read from the map itself), the personal
 * Player 1 camera preference, and the list of the levels of the project (double-click to save and open one).
 */
class SWB_GymSelectorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_GymSelectorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWB_GymSelectorPanel() override;

private:
	void Refresh();
	void ApplyState(const FWB_GymModeState& NewState);
	void OnGymModeChanged(ECheckBoxState NewState);
	void OnGymSelected(TSharedPtr<FWB_GymListItem> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWB_GymListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnMapOpened(const FString& Filename, bool bAsTemplate);
	void OnEndPIE(bool bIsSimulating);

	/** Levels: rebuilt from the Asset Registry, then filtered by the search text. */
	void RefreshLevels();
	void ApplyLevelFilter();
	TSharedRef<ITableRow> OnGenerateLevelRow(TSharedPtr<FWB_LevelListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnLevelDoubleClicked(TSharedPtr<FWB_LevelListItem> Item);

	FWB_GymModeState State;
	TArray<TSharedPtr<FWB_GymListItem>> Gyms;
	TSharedPtr<SListView<TSharedPtr<FWB_GymListItem>>> ListView;
	TSharedPtr<STextBlock> MapText;
	TSharedPtr<STextBlock> WarningsText;

	TArray<TSharedPtr<FWB_LevelListItem>> AllLevels;
	TArray<TSharedPtr<FWB_LevelListItem>> LevelRows;
	TSharedPtr<SListView<TSharedPtr<FWB_LevelListItem>>> LevelList;
	FString LevelFilter;
};
