// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Collections/SWB_CollectionsPanel.h"

#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "WBCollectionsPanel"

namespace WBCollectionsPanel
{
	const FName ColumnChange(TEXT("Change"));
	const FName ColumnCollection(TEXT("Collection"));
	const FName ColumnAsset(TEXT("Asset"));

	class SChangeRow : public SMultiColumnTableRow<TSharedPtr<FWB_CollectionChange>>
	{
	public:
		SLATE_BEGIN_ARGS(SChangeRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FWB_CollectionChange> InItem)
		{
			Item = InItem;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == ColumnChange)
			{
				return SNew(STextBlock)
					.Text(Item->bAdded ? LOCTEXT("Added", "+ add") : LOCTEXT("Removed", "- remove"))
					.ColorAndOpacity(Item->bAdded ? FSlateColor::UseForeground() : FSlateColor(WBToolsStyle::Warning));
			}
			if (ColumnName == ColumnCollection)
			{
				return SNew(STextBlock).Text(FText::FromString(Item->Collection));
			}
			const FString Name = FPaths::GetBaseFilename(Item->Asset);
			return SNew(STextBlock).Text(FText::FromString(Name)).ToolTipText(FText::FromString(Item->Asset)).ColorAndOpacity(WBToolsStyle::Dim);
		}

	private:
		TSharedPtr<FWB_CollectionChange> Item;
	};
}

void SWB_CollectionsPanel::Construct(const FArguments& InArgs)
{
	using namespace WBCollectionsPanel;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("Help", "Builds your local collections from the collection tags (Collection.*). Check the master tags you want to see: unchecked ones are removed from your collections. Tag a Blueprint or a Data Asset with the Collections button of its editor, or right-click > Collection Tags... in the Content Browser."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SNew(STextBlock).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))).Text(LOCTEXT("Masters", "Master tags"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 4.0f, 8.0f, 8.0f)
		[
			SAssignNew(MasterTagsBox, SVerticalBox)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Preview", "Preview"))
				.ToolTipText(LOCTEXT("PreviewTip", "Lists what the update would change, without changing your collections."))
				.OnClicked_Lambda([this]() { ShowReport(UWB_CollectionsLibrary::PreviewCollections()); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Update", "Update Collections"))
				.ToolTipText(LOCTEXT("UpdateTip", "Builds your local collections from the tags."))
				.OnClicked_Lambda([this]() { ShowReport(UWB_CollectionsLibrary::UpdateCollections()); return FReply::Handled(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SAssignNew(SummaryText, STextBlock).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(140.0f).Padding(8.0f, 0.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(WarningsText, STextBlock).AutoWrapText(true).ColorAndOpacity(WBToolsStyle::Warning)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FWB_CollectionChange>>)
			.ListItemsSource(&Rows)
			.OnGenerateRow(this, &SWB_CollectionsPanel::OnGenerateRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ColumnChange).DefaultLabel(LOCTEXT("ColChange", "Change")).FillWidth(0.12f)
				+ SHeaderRow::Column(ColumnCollection).DefaultLabel(LOCTEXT("ColCollection", "Collection")).FillWidth(0.25f)
				+ SHeaderRow::Column(ColumnAsset).DefaultLabel(LOCTEXT("ColAsset", "Asset")).FillWidth(0.63f)
			)
		]
	];
	RebuildMasterTags();
	SummaryText->SetText(LOCTEXT("NoPreview", "Check the master tags, then Preview or Update Collections."));
}

void SWB_CollectionsPanel::RebuildMasterTags()
{
	MasterTagsBox->ClearChildren();
	const TArray<FGameplayTag> Masters = UWB_CollectionsLibrary::GetMasterTags();
	if (Masters.Num() == 0)
	{
		MasterTagsBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock).ColorAndOpacity(WBToolsStyle::Warning).Text(LOCTEXT("NoMasters", "No master tag: add children to the collection root tag (Project Settings > Gameplay Tags)."))
		];
		return;
	}
	for (const FGameplayTag& Master : Masters)
	{
		MasterTagsBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Master]() { return UWB_CollectionsLibrary::IsMasterTagEnabled(Master) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Master](ECheckBoxState State) { UWB_CollectionsLibrary::SetMasterTagEnabled(Master, State == ECheckBoxState::Checked); })
			[
				SNew(STextBlock).Text(FText::FromString(Master.ToString()))
			]
		];
	}
}

void SWB_CollectionsPanel::ShowReport(const FWB_CollectionsReport& Report)
{
	Rows.Reset();
	int32 NumAdded = 0;
	for (const FWB_CollectionChange& Change : Report.Changes)
	{
		Rows.Add(MakeShared<FWB_CollectionChange>(Change));
		NumAdded += Change.bAdded ? 1 : 0;
	}
	ListView->RequestListRefresh();

	SummaryText->SetText(FText::Format(LOCTEXT("Summary", "{0}: {1} collection(s), {2} created, {3} removed, {4} asset(s) added, {5} removed. Details in the Output Log (LogWBCollections)."),
		Report.bApplied ? LOCTEXT("Updated", "Updated") : LOCTEXT("PreviewDone", "Preview"),
		Report.NumCollections, Report.CreatedCollections.Num(), Report.RemovedCollections.Num(), NumAdded, Report.Changes.Num() - NumAdded));

	TArray<FString> Lines = Report.Warnings;
	if (Report.UntaggedClasses.Num() > 0)
	{
		Lines.Add(TEXT("Data Asset classes without collection tag (their assets go in Untagged): ") + FString::Join(Report.UntaggedClasses, TEXT(", ")));
	}
	if (Report.RemovedCollections.Num() > 0)
	{
		Lines.Add(TEXT("Removed (master tag unchecked): ") + FString::Join(Report.RemovedCollections, TEXT(", ")));
	}
	WarningsText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

TSharedRef<ITableRow> SWB_CollectionsPanel::OnGenerateRow(TSharedPtr<FWB_CollectionChange> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(WBCollectionsPanel::SChangeRow, OwnerTable, Item);
}

#undef LOCTEXT_NAMESPACE
