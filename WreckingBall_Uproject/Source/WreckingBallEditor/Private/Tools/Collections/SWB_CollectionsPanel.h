// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/Collections/WB_CollectionsLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STextBlock;
class SVerticalBox;

/** WB Tools > Content > Collections: master tags to show, preview, update of the local collections. */
class SWB_CollectionsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_CollectionsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RebuildMasterTags();
	void ShowReport(const FWB_CollectionsReport& Report);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWB_CollectionChange> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<SVerticalBox> MasterTagsBox;
	TSharedPtr<STextBlock> SummaryText;
	TSharedPtr<STextBlock> WarningsText;
	TArray<TSharedPtr<FWB_CollectionChange>> Rows;
	TSharedPtr<SListView<TSharedPtr<FWB_CollectionChange>>> ListView;
};
