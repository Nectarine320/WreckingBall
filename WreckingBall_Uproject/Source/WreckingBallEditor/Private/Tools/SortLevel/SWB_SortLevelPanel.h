// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/SortLevel/WB_SortLevelLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STextBlock;

/** WB Tools > Level > Sort Level: preview of the moves, then apply. */
class SWB_SortLevelPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_SortLevelPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnPreviewClicked();
	FReply OnApplyClicked();
	void ShowReport(const FWB_SortLevelReport& Report);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWB_SortLevelMove> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<TSharedPtr<FWB_SortLevelMove>> Rows;
	TSharedPtr<SListView<TSharedPtr<FWB_SortLevelMove>>> ListView;
	TSharedPtr<STextBlock> SummaryText;
	TSharedPtr<STextBlock> WarningsText;
	TSharedPtr<STextBlock> GroupsText;
	bool bHasPreview = false;
};
