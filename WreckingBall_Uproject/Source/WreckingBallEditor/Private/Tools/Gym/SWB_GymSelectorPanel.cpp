// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Gym/SWB_GymSelectorPanel.h"

#include "Editor.h"
#include "Gym/WB_GymDefinition.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "TimerManager.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WBGymSelectorPanel"

namespace WBLevelList
{
	const FName ColumnLevel(TEXT("Level"));
	const FName ColumnSubLevels(TEXT("SubLevels"));
	const FName ColumnFolder(TEXT("Folder"));

	/** A group header (text in the first column) or a map: name (bold yellow when open), sub-levels, folder and magnifier. */
	class SLevelRow : public SMultiColumnTableRow<TSharedPtr<FWB_LevelListItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SLevelRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FWB_LevelListItem> InItem, bool bInOpen)
		{
			Item = InItem;
			bOpen = bInOpen;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.0f, Item->IsHeader() ? 8.0f : 1.0f, 0.0f, 1.0f)), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (Item->IsHeader())
			{
				if (ColumnName != ColumnLevel)
				{
					return SNullWidget::NullWidget;
				}
				return SNew(STextBlock).Text(Item->Header).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")));
			}
			if (ColumnName == ColumnLevel)
			{
				const FString Name = FPackageName::GetShortName(Item->MapPackage);
				return SNew(STextBlock)
					.Margin(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
					.Text(FText::FromString(bOpen ? Name + TEXT("   (open)") : Name))
					.Font(bOpen ? FAppStyle::GetFontStyle(TEXT("BoldFont")) : FAppStyle::GetFontStyle(TEXT("NormalFont")))
					.ColorAndOpacity(bOpen ? FSlateColor(WBToolsStyle::Highlight) : FSlateColor::UseForeground())
					.ToolTipText(FText::FromString(Item->MapPackage));
			}
			if (ColumnName == ColumnSubLevels)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->SubLevels.IsEmpty() ? TEXT("-") : Item->SubLevels))
					.ColorAndOpacity(WBToolsStyle::Dim)
					.ToolTipText(FText::FromString(Item->SubLevels));
			}
			const FString MapPackage = Item->MapPackage;
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(FPackageName::GetLongPackagePath(MapPackage))).ColorAndOpacity(WBToolsStyle::Dim)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
					.ToolTipText(LOCTEXT("BrowseLevelTip", "Select the map in the Content Browser (Ctrl+B)."))
					.OnClicked_Lambda([MapPackage]() { UWB_GymSelectorLibrary::BrowseToLevel(MapPackage); return FReply::Handled(); })
					[
						SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Search"))).ColorAndOpacity(FSlateColor::UseForeground())
					]
				];
		}

	private:
		TSharedPtr<FWB_LevelListItem> Item;
		bool bOpen = false;
	};
}


void SWB_GymSelectorPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SAssignNew(MapText, STextBlock).AutoWrapText(true).ColorAndOpacity(WBToolsStyle::Dim)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("GymModeTip", "On: the GameMode Override becomes the gym GameMode and BP_GymSettings chooses the gym. Off: the map gets back its GameMode Override; BP_GymSettings stays, inert."))
				.IsChecked_Lambda([this]() { return State.bGymModeEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged(this, &SWB_GymSelectorPanel::OnGymModeChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("GymMode", "Gym mode enabled"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTip", "Reads the map, the gym list and the level list again."))
				.OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f, 8.0f, 0.0f)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("Player1CameraTip", "On: every Play In Editor starts with the Player 1 camera, in any map, gym mode or not. Personal preference of this editor: never saved in a map, never committed."))
			.IsChecked_Lambda([this]() { return State.bPlayer1Camera ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { ApplyState(UWB_GymSelectorLibrary::SetPlayer1Camera(NewState == ECheckBoxState::Checked)); })
			[
				SNew(STextBlock).Text(LOCTEXT("Player1Camera", "Camera Player 1 (personal, every Play)"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(WBToolsStyle::Error)
			.Visibility_Lambda([this]() { return State.bShowCommitReminder ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("Reminder", "This map is not a gym map: turn gym mode off before committing it."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SAssignNew(WarningsText, STextBlock).AutoWrapText(true).ColorAndOpacity(WBToolsStyle::Warning)
		]
		// ---------------------------------------------------------------- Gyms (gym mode on only)
		+ SVerticalBox::Slot().FillHeight(0.4f).Padding(8.0f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]() { return State.bGymModeEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))).Text(LOCTEXT("ChooseGym", "Gym (applied at once, used by the next Play)"))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FWB_GymListItem>>)
				.ListItemsSource(&Gyms)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SWB_GymSelectorPanel::OnGenerateRow)
				.OnSelectionChanged(this, &SWB_GymSelectorPanel::OnGymSelected)
			]
		]
		// ---------------------------------------------------------------- Levels
		+ SVerticalBox::Slot().FillHeight(0.6f).Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))).Text(LOCTEXT("Levels", "Levels (double-click to open: unsaved changes are asked first)"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("LevelFilter", "Filter the levels"))
				.OnTextChanged_Lambda([this](const FText& Text) { LevelFilter = Text.ToString(); ApplyLevelFilter(); })
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(LevelList, SListView<TSharedPtr<FWB_LevelListItem>>)
				.ListItemsSource(&LevelRows)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SWB_GymSelectorPanel::OnGenerateLevelRow)
				.OnMouseButtonDoubleClick(this, &SWB_GymSelectorPanel::OnLevelDoubleClicked)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(WBLevelList::ColumnLevel).DefaultLabel(LOCTEXT("ColLevel", "Level")).FillWidth(0.3f)
					+ SHeaderRow::Column(WBLevelList::ColumnSubLevels).DefaultLabel(LOCTEXT("ColSubLevels", "Sub-levels")).FillWidth(0.3f)
					+ SHeaderRow::Column(WBLevelList::ColumnFolder).DefaultLabel(LOCTEXT("ColFolder", "Folder")).FillWidth(0.4f)
				)
			]
		]
	];

	FEditorDelegates::OnMapOpened.AddSP(this, &SWB_GymSelectorPanel::OnMapOpened);
	FEditorDelegates::EndPIE.AddSP(this, &SWB_GymSelectorPanel::OnEndPIE);
	Refresh();
}

SWB_GymSelectorPanel::~SWB_GymSelectorPanel()
{
	FEditorDelegates::OnMapOpened.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
}

void SWB_GymSelectorPanel::Refresh()
{
	Gyms.Reset();
	for (UWB_GymDefinition* Gym : UWB_GymSelectorLibrary::GetGymDefinitions())
	{
		TSharedPtr<FWB_GymListItem> Item = MakeShared<FWB_GymListItem>();
		Item->Gym = Gym;
		const FString Name = Gym->GymName.IsEmpty() ? Gym->GetName() : FString::Printf(TEXT("%s   (%s)"), *Gym->GymName.ToString(), *Gym->GetName());
		Item->Label = FText::FromString(Name);
		Gyms.Add(Item);
	}
	ListView->RequestListRefresh();
	ApplyState(UWB_GymSelectorLibrary::GetGymModeState());
	RefreshLevels();
}

void SWB_GymSelectorPanel::ApplyState(const FWB_GymModeState& NewState)
{
	State = NewState;
	MapText->SetText(FText::Format(LOCTEXT("Map", "Map: {0}   |   GameMode Override: {1}"), FText::FromString(State.MapName), FText::FromString(State.GameModeOverride)));
	TArray<FString> Warnings = State.Warnings;
	if (Gyms.Num() == 0)
	{
		Warnings.Add(TEXT("No gym definition (UWB_GymDefinition) in the project."));
	}
	WarningsText->SetText(FText::FromString(FString::Join(Warnings, TEXT("\n"))));

	// Show the gym of the map, without applying it again.
	TSharedPtr<FWB_GymListItem> Selected;
	for (const TSharedPtr<FWB_GymListItem>& Item : Gyms)
	{
		if (State.ActiveGym && Item->Gym.Get() == State.ActiveGym)
		{
			Selected = Item;
		}
	}
	if (Selected)
	{
		ListView->SetSelection(Selected, ESelectInfo::Direct);
	}
	else
	{
		ListView->ClearSelection();
	}
	if (LevelList.IsValid())
	{
		// The open map is highlighted: rebuild the rows.
		LevelList->RebuildList();
	}
}

void SWB_GymSelectorPanel::OnGymModeChanged(ECheckBoxState NewState)
{
	const bool bEnable = NewState == ECheckBoxState::Checked;
	ApplyState(UWB_GymSelectorLibrary::SetGymModeEnabled(bEnable, nullptr));
}

void SWB_GymSelectorPanel::OnGymSelected(TSharedPtr<FWB_GymListItem> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct || !Item || !Item->Gym.IsValid() || Item->Gym.Get() == State.ActiveGym)
	{
		return;
	}
	ApplyState(UWB_GymSelectorLibrary::SetActiveGym(Item->Gym.Get()));
}

TSharedRef<ITableRow> SWB_GymSelectorPanel::OnGenerateRow(TSharedPtr<FWB_GymListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FWB_GymListItem>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(STextBlock).Text(Item->Label)
		];
}

void SWB_GymSelectorPanel::RefreshLevels()
{
	AllLevels.Reset();
	for (const FString& MapPackage : UWB_GymSelectorLibrary::GetLevelList())
	{
		TSharedPtr<FWB_LevelListItem> Item = MakeShared<FWB_LevelListItem>();
		Item->MapPackage = MapPackage;
		Item->Category = UWB_GymSelectorLibrary::GetLevelCategory(FPackageName::GetShortName(MapPackage));
		TArray<FString> SubLevels = UWB_GymSelectorLibrary::GetSubLevels(MapPackage);
		for (FString& SubLevel : SubLevels)
		{
			// SubLv_000_Soccerfield_Layout -> Layout: the part after the map name is enough here.
			int32 LastUnderscore = INDEX_NONE;
			SubLevel = SubLevel.FindLastChar(TEXT('_'), LastUnderscore) ? SubLevel.Mid(LastUnderscore + 1) : SubLevel;
		}
		Item->SubLevels = FString::Join(SubLevels, TEXT(", "));
		AllLevels.Add(Item);
	}
	ApplyLevelFilter();
}

void SWB_GymSelectorPanel::ApplyLevelFilter()
{
	// The library sorts by group: add a header before the first map of each group, with its count.
	LevelRows.Reset();
	TSharedPtr<FWB_LevelListItem> Header;
	int32 Count = 0;
	auto CloseHeader = [&Header, &Count]()
	{
		if (Header)
		{
			const FText Name = Header->Category == EWB_LevelCategory::Official ? LOCTEXT("Official", "Official levels")
				: Header->Category == EWB_LevelCategory::Gym ? LOCTEXT("GymLevels", "Gyms")
				: LOCTEXT("Unsorted", "Unsorted");
			Header->Header = FText::Format(LOCTEXT("HeaderCount", "{0}  ({1})"), Name, Count);
		}
	};
	for (const TSharedPtr<FWB_LevelListItem>& Item : AllLevels)
	{
		if (!LevelFilter.IsEmpty() && !Item->MapPackage.Contains(LevelFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Header || Header->Category != Item->Category)
		{
			CloseHeader();
			Header = MakeShared<FWB_LevelListItem>();
			Header->Category = Item->Category;
			LevelRows.Add(Header);
			Count = 0;
		}
		LevelRows.Add(Item);
		++Count;
	}
	CloseHeader();
	LevelList->RebuildList();
}

TSharedRef<ITableRow> SWB_GymSelectorPanel::OnGenerateLevelRow(TSharedPtr<FWB_LevelListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(WBLevelList::SLevelRow, OwnerTable, Item, Item->MapPackage == State.MapName);
}

void SWB_GymSelectorPanel::OnLevelDoubleClicked(TSharedPtr<FWB_LevelListItem> Item)
{
	if (Item && !Item->IsHeader() && Item->MapPackage != State.MapName)
	{
		// The map change refreshes the panel through OnMapOpened.
		UWB_GymSelectorLibrary::OpenLevel(Item->MapPackage);
	}
}

void SWB_GymSelectorPanel::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void SWB_GymSelectorPanel::OnEndPIE(bool bIsSimulating)
{
	// The play world is still alive during EndPIE: read the map on the next tick.
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SWB_GymSelectorPanel::Refresh));
	}
}

#undef LOCTEXT_NAMESPACE
