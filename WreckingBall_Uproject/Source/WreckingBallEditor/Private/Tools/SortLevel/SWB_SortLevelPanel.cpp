// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/SortLevel/SWB_SortLevelPanel.h"

#include "Misc/MessageDialog.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "WBSortLevelPanel"

namespace WBSortLevelPanel
{
	const FName ColumnActor(TEXT("Actor"));
	const FName ColumnFrom(TEXT("From"));
	const FName ColumnTo(TEXT("To"));
	const FName ColumnWarning(TEXT("Warning"));

	class SMoveRow : public SMultiColumnTableRow<TSharedPtr<FWB_SortLevelMove>>
	{
	public:
		SLATE_BEGIN_ARGS(SMoveRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FWB_SortLevelMove> InItem)
		{
			Item = InItem;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			const FWB_SortLevelMove& Move = *Item;
			FString Text;
			FSlateColor Color = FSlateColor::UseForeground();
			if (ColumnName == ColumnActor)
			{
				Text = FString::Printf(TEXT("%s  (%s)"), *Move.ActorLabel, *Move.ActorClass);
				Color = Move.bUnsorted ? FSlateColor(WBToolsStyle::Warning) : Color;
			}
			else if (ColumnName == ColumnFrom)
			{
				Text = FString::Printf(TEXT("%s  |  %s"), Move.FromFolder.IsEmpty() ? TEXT("(root)") : *Move.FromFolder, *Move.FromLevel);
				Color = FSlateColor(WBToolsStyle::Dim);
			}
			else if (ColumnName == ColumnTo)
			{
				Text = FString::Printf(TEXT("%s  |  %s"), *Move.ToFolder, *Move.ToLevel);
			}
			else
			{
				Text = Move.Warning;
				Color = FSlateColor(WBToolsStyle::Warning);
			}
			return SNew(STextBlock).Text(FText::FromString(Text)).ColorAndOpacity(Color).ToolTipText(FText::FromString(Text));
		}

	private:
		TSharedPtr<FWB_SortLevelMove> Item;
	};
}

void SWB_SortLevelPanel::Construct(const FArguments& InArgs)
{
	using namespace WBSortLevelPanel;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("Help", "Puts every actor of the open map in its Outliner folder and sub-level (rules: Project Settings > Editor > WB Tools > Sort Level Rules). Grouped and attached actors are never moved. Preview first, then Apply, then save the map."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Preview", "Preview"))
				.ToolTipText(LOCTEXT("PreviewTip", "Lists what would move, without changing the map."))
				.OnClicked(this, &SWB_SortLevelPanel::OnPreviewClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Apply", "Apply"))
				.ToolTipText(LOCTEXT("ApplyTip", "Moves the actors listed by the preview."))
				.IsEnabled_Lambda([this]() { return bHasPreview; })
				.OnClicked(this, &SWB_SortLevelPanel::OnApplyClicked)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SAssignNew(SummaryText, STextBlock).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SAssignNew(WarningsText, STextBlock).AutoWrapText(true).ColorAndOpacity(WBToolsStyle::Warning)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FWB_SortLevelMove>>)
			.ListItemsSource(&Rows)
			.OnGenerateRow(this, &SWB_SortLevelPanel::OnGenerateRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ColumnActor).DefaultLabel(LOCTEXT("ColActor", "Actor")).FillWidth(0.25f)
				+ SHeaderRow::Column(ColumnFrom).DefaultLabel(LOCTEXT("ColFrom", "From (folder | level)")).FillWidth(0.25f)
				+ SHeaderRow::Column(ColumnTo).DefaultLabel(LOCTEXT("ColTo", "To (folder | level)")).FillWidth(0.25f)
				+ SHeaderRow::Column(ColumnWarning).DefaultLabel(LOCTEXT("ColWarning", "To check")).FillWidth(0.25f)
			)
		]
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(160.0f).Padding(8.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(GroupsText, STextBlock).AutoWrapText(true).ColorAndOpacity(WBToolsStyle::Dim)
			]
		]
	];
	SummaryText->SetText(LOCTEXT("NoPreview", "Click Preview to read the map."));
}

FReply SWB_SortLevelPanel::OnPreviewClicked()
{
	const FWB_SortLevelReport Report = UWB_SortLevelLibrary::PreviewSortLevel();
	bHasPreview = Report.Moves.Num() > 0;
	ShowReport(Report);
	return FReply::Handled();
}

FReply SWB_SortLevelPanel::OnApplyClicked()
{
	const FText Question = FText::Format(LOCTEXT("Confirm", "Move {0} actor(s) to their folder and sub-level? Folder changes can be undone; level moves may not. Check the \"To check\" column first."), Rows.Num());
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}
	const FWB_SortLevelReport Report = UWB_SortLevelLibrary::ApplySortLevel();
	bHasPreview = false;
	ShowReport(Report);
	return FReply::Handled();
}

void SWB_SortLevelPanel::ShowReport(const FWB_SortLevelReport& Report)
{
	Rows.Reset();
	int32 NumUnsorted = 0;
	for (const FWB_SortLevelMove& Move : Report.Moves)
	{
		Rows.Add(MakeShared<FWB_SortLevelMove>(Move));
		NumUnsorted += Move.bUnsorted ? 1 : 0;
	}
	ListView->RequestListRefresh();

	SummaryText->SetText(FText::Format(LOCTEXT("Summary", "{0}: {1} actor(s) to move, {2} unsorted, {3} already in place, {4} group(s) or attached actor(s) skipped. Details in the Output Log (LogWBSortLevel)."),
		Report.bApplied ? LOCTEXT("Applied", "Applied") : LOCTEXT("PreviewDone", "Preview"),
		Report.Moves.Num() - NumUnsorted, NumUnsorted, Report.NumAlreadySorted, Report.SkippedGroups.Num()));
	WarningsText->SetText(FText::FromString(FString::Join(Report.Warnings, TEXT("\n"))));
	GroupsText->SetText(Report.SkippedGroups.Num() > 0
		? FText::FromString(TEXT("Skipped (never moved):\n") + FString::Join(Report.SkippedGroups, TEXT("\n")))
		: FText::GetEmpty());
}

TSharedRef<ITableRow> SWB_SortLevelPanel::OnGenerateRow(TSharedPtr<FWB_SortLevelMove> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(WBSortLevelPanel::SMoveRow, OwnerTable, Item);
}

#undef LOCTEXT_NAMESPACE
