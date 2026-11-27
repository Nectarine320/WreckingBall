// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/DebugKeys/SWB_DebugKeysPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "WBDebugKeysPanel"

namespace WBDebugKeysPanel
{
	const FName ColumnKey(TEXT("Key"));
	const FName ColumnDescription(TEXT("Description"));
	const FName ColumnWhere(TEXT("Where"));
	const FName ColumnConflict(TEXT("Conflict"));

	class SKeyRow : public SMultiColumnTableRow<TSharedPtr<FWB_DebugKeyEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SKeyRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FWB_DebugKeyEntry> InItem)
		{
			Item = InItem;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			const FWB_DebugKeyEntry& Entry = *Item;
			if (ColumnName == ColumnDescription)
			{
				return MakeDescription();
			}
			if (ColumnName == ColumnWhere)
			{
				return MakeWhere();
			}
			const bool bKey = ColumnName == ColumnKey;
			const FString Text = bKey ? Entry.Key : (Entry.Conflict.IsEmpty() ? FString() : TEXT("Also: ") + Entry.Conflict);
			const FSlateColor Color = !bKey || !Entry.Conflict.IsEmpty() ? FSlateColor(WBToolsStyle::Error) : FSlateColor::UseForeground();
			const FText ToolTip = bKey && Entry.bConsoleCommand ? LOCTEXT("CopyTip", "Double-click to copy the command.") : FText::FromString(Text);
			return SNew(STextBlock).Text(FText::FromString(Text)).ColorAndOpacity(Color).ToolTipText(ToolTip);
		}

	private:
		/** Console help is read-only (C++). A Blueprint key gets a text box that writes the node comment. */
		TSharedRef<SWidget> MakeDescription()
		{
			const FWB_DebugKeyEntry& Entry = *Item;
			if (Entry.bConsoleCommand)
			{
				const FString Text = Entry.Description.IsEmpty() ? TEXT("(no help text in C++)") : Entry.Description;
				return SNew(STextBlock).Text(FText::FromString(Text)).ToolTipText(FText::FromString(Text))
					.ColorAndOpacity(Entry.Description.IsEmpty() ? FSlateColor(WBToolsStyle::Warning) : FSlateColor::UseForeground());
			}

			const FText Hint = Entry.CommentBoxTitle.IsEmpty()
				? LOCTEXT("HintNone", "No description: type what the key does, then Enter")
				: FText::Format(LOCTEXT("HintBox", "No description (comment box: \"{0}\"): type what the key does, then Enter"), FText::FromString(Entry.CommentBoxTitle));
			TWeakPtr<FWB_DebugKeyEntry> WeakItem = Item;
			return SNew(SEditableTextBox)
				.Text(FText::FromString(Entry.Description))
				.HintText(Hint)
				.ToolTipText(LOCTEXT("DescriptionTip", "Written in the comment of the node (pinned bubble in the graph). Save the Blueprint after."))
				.BackgroundColor_Lambda([WeakItem]()
				{
					const TSharedPtr<FWB_DebugKeyEntry> Pinned = WeakItem.Pin();
					return Pinned && Pinned->Description.IsEmpty() ? FSlateColor(WBToolsStyle::Warning * 0.35f) : FSlateColor(FLinearColor::White * 0.1f);
				})
				.OnTextCommitted_Lambda([WeakItem](const FText& NewText, ETextCommit::Type CommitType)
				{
					const TSharedPtr<FWB_DebugKeyEntry> Pinned = WeakItem.Pin();
					const FString Text = NewText.ToString().TrimStartAndEnd();
					if (!Pinned || CommitType == ETextCommit::OnCleared || Text == Pinned->Description)
					{
						return;
					}
					if (UWB_DebugKeysLibrary::SetDebugKeyDescription(Pinned->Asset, Pinned->NodeGuid, Text))
					{
						Pinned->Description = Text;
					}
				});
		}

		/** Asset name, then a magnifier (select in the Content Browser) and a pencil (open the Blueprint on the node). */
		TSharedRef<SWidget> MakeWhere()
		{
			const FWB_DebugKeyEntry& Entry = *Item;
			if (Entry.bConsoleCommand)
			{
				return SNew(STextBlock).Text(LOCTEXT("Console", "Console command (C++)")).ColorAndOpacity(WBToolsStyle::Dim);
			}
			const FString Asset = Entry.Asset;
			const FGuid NodeGuid = Entry.NodeGuid;
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s  (%s)"), *FPackageName::GetShortName(Asset), *Entry.Graph)))
					.ToolTipText(FText::FromString(Asset))
					.ColorAndOpacity(WBToolsStyle::Dim)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
					.ToolTipText(LOCTEXT("BrowseTip", "Select the Blueprint in the Content Browser (Ctrl+B)."))
					.OnClicked_Lambda([Asset]() { UWB_DebugKeysLibrary::BrowseToDebugKey(Asset); return FReply::Handled(); })
					[
						SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Search"))).ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
					.ToolTipText(LOCTEXT("EditTip", "Open the Blueprint, centered on the key node."))
					.OnClicked_Lambda([Asset, NodeGuid]() { UWB_DebugKeysLibrary::OpenDebugKey(Asset, NodeGuid); return FReply::Handled(); })
					[
						SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Edit"))).ColorAndOpacity(FSlateColor::UseForeground())
					]
				];
		}

		TSharedPtr<FWB_DebugKeyEntry> Item;
	};
}

void SWB_DebugKeysPanel::Construct(const FArguments& InArgs)
{
	using namespace WBDebugKeysPanel;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(SummaryText, STextBlock).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTip", "Scans the project again."))
				.OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CopyMarkdown", "Copy as Markdown"))
				.ToolTipText(LOCTEXT("CopyMarkdownTip", "Copies the list as a Markdown table, to paste it in a project doc."))
				.OnClicked(this, &SWB_DebugKeysPanel::OnCopyMarkdown)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(WBToolsStyle::Dim)
			.Text(LOCTEXT("Help", "Type a description in the Description column and press Enter: it is written in the comment of the node. Save the Blueprint after. Magnifier: select the Blueprint. Pencil: open it on the node."))
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot().Value(0.7f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))).Text(LOCTEXT("KeysTitle", "Debug keys (keyboard shortcuts in Blueprints)"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SAssignNew(KeyList, SListView<TSharedPtr<FWB_DebugKeyEntry>>)
					.ListItemsSource(&KeyRows)
					.OnGenerateRow(this, &SWB_DebugKeysPanel::OnGenerateRow)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColumnKey).DefaultLabel(LOCTEXT("ColKey", "Key")).FillWidth(0.12f)
						+ SHeaderRow::Column(ColumnDescription).DefaultLabel(LOCTEXT("ColDescription", "Description")).FillWidth(0.43f)
						+ SHeaderRow::Column(ColumnWhere).DefaultLabel(LOCTEXT("ColWhere", "Where (asset)")).FillWidth(0.27f)
						+ SHeaderRow::Column(ColumnConflict).DefaultLabel(LOCTEXT("ColConflict", "Conflict")).FillWidth(0.18f)
					)
				]
			]
			+ SSplitter::Slot().Value(0.3f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))).Text(LOCTEXT("CommandsTitle", "Debug commands (double-click a command to copy it, then paste it in the console)"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SAssignNew(CommandList, SListView<TSharedPtr<FWB_DebugKeyEntry>>)
					.ListItemsSource(&CommandRows)
					.OnGenerateRow(this, &SWB_DebugKeysPanel::OnGenerateRow)
					.OnMouseButtonDoubleClick_Lambda([](TSharedPtr<FWB_DebugKeyEntry> Item)
					{
						if (!Item)
						{
							return;
						}
						FPlatformApplicationMisc::ClipboardCopy(*Item->Key);
						FNotificationInfo Info(FText::Format(LOCTEXT("Copied", "{0} copied: paste it in the console."), FText::FromString(Item->Key)));
						Info.ExpireDuration = 2.0f;
						FSlateNotificationManager::Get().AddNotification(Info);
					})
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColumnKey).DefaultLabel(LOCTEXT("ColCommand", "Command")).FillWidth(0.25f)
						+ SHeaderRow::Column(ColumnDescription).DefaultLabel(LOCTEXT("ColHelp", "Help (from C++)")).FillWidth(0.75f)
					)
				]
			]
		]
	];
	Refresh();
}

void SWB_DebugKeysPanel::Refresh()
{
	KeyRows.Reset();
	CommandRows.Reset();
	int32 NumConflicts = 0;
	int32 NumUndocumented = 0;
	for (const FWB_DebugKeyEntry& Entry : UWB_DebugKeysLibrary::ScanDebugKeys())
	{
		(Entry.bConsoleCommand ? CommandRows : KeyRows).Add(MakeShared<FWB_DebugKeyEntry>(Entry));
		NumConflicts += Entry.Conflict.IsEmpty() ? 0 : 1;
		NumUndocumented += !Entry.bConsoleCommand && Entry.Description.IsEmpty() ? 1 : 0;
	}
	KeyList->RebuildList();
	CommandList->RebuildList();
	SummaryText->SetText(FText::Format(LOCTEXT("Summary", "{0} debug key(s), {1} debug command(s). {2} key(s) without description (orange), {3} in conflict (red)."),
		KeyRows.Num(), CommandRows.Num(), NumUndocumented, NumConflicts));
}

FReply SWB_DebugKeysPanel::OnCopyMarkdown()
{
	// Built from the rows, so that descriptions typed since the last scan are included.
	TArray<FWB_DebugKeyEntry> Entries;
	for (const TSharedPtr<FWB_DebugKeyEntry>& Row : KeyRows)
	{
		Entries.Add(*Row);
	}
	for (const TSharedPtr<FWB_DebugKeyEntry>& Row : CommandRows)
	{
		Entries.Add(*Row);
	}
	FPlatformApplicationMisc::ClipboardCopy(*UWB_DebugKeysLibrary::ToMarkdown(Entries));
	return FReply::Handled();
}

TSharedRef<ITableRow> SWB_DebugKeysPanel::OnGenerateRow(TSharedPtr<FWB_DebugKeyEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(WBDebugKeysPanel::SKeyRow, OwnerTable, Item);
}

#undef LOCTEXT_NAMESPACE
