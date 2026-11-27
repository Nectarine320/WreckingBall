// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/DebugKeys/WB_DebugKeysLibrary.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STextBlock;

/** WB Tools > Debug > Debug Keys & Commands: debug keys (Blueprint keyboard nodes) and debug commands (WB.* console), rebuilt at each opening. */
class SWB_DebugKeysPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_DebugKeysPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Refresh();
	FReply OnCopyMarkdown();
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWB_DebugKeyEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<TSharedPtr<FWB_DebugKeyEntry>> KeyRows;
	TArray<TSharedPtr<FWB_DebugKeyEntry>> CommandRows;
	TSharedPtr<SListView<TSharedPtr<FWB_DebugKeyEntry>>> KeyList;
	TSharedPtr<SListView<TSharedPtr<FWB_DebugKeyEntry>>> CommandList;
	TSharedPtr<STextBlock> SummaryText;
};
