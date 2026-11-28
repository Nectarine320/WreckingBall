// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Menu/WB_ToolsMenu.h"

#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Retarget/SWB_RetargetPanel.h"
#include "Tools/Collections/SWB_CollectionsPanel.h"
#include "Tools/LevelCreator/SWB_LevelCreatorPanel.h"
#include "Tools/DebugKeys/SWB_DebugKeysPanel.h"
#include "Tools/Gym/SWB_GymSelectorPanel.h"
#include "Tools/Menu/WB_ToolsMenuData.h"
#include "Tools/SortLevel/SWB_SortLevelPanel.h"
#include "Tools/WB_ToolsSettings.h"
#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBToolsMenu, Log, All);

#define LOCTEXT_NAMESPACE "WBToolsMenu"

const FName FWB_ToolsMenu::SortLevelTabId(TEXT("WBTools_SortLevel"));
const FName FWB_ToolsMenu::GymTabId(TEXT("WBTools_Gym"));
const FName FWB_ToolsMenu::DebugKeysTabId(TEXT("WBTools_DebugKeys"));
const FName FWB_ToolsMenu::CollectionsTabId(TEXT("WBTools_Collections"));
const FName FWB_ToolsMenu::RetargetTabId(TEXT("WBTools_Retarget"));
const FName FWB_ToolsMenu::LevelCreatorTabId(TEXT("WBTools_LevelCreator"));

namespace WBToolsMenu
{
	const FName OwnerName(TEXT("WreckingBallEditor.WBTools"));
	FDelegateHandle StartupHandle;

	template <typename TPanel>
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(TPanel)];
	}

	void RunEntry(const FWB_ToolsMenuEntry& Entry)
	{
		if (Entry.Action == EWB_ToolsMenuAction::EditorUtilityWidget)
		{
			UEditorUtilityWidgetBlueprint* Widget = Entry.Widget.LoadSynchronous();
			UEditorUtilitySubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
			if (!Widget || !Subsystem)
			{
				UE_LOG(LogWBToolsMenu, Warning, TEXT("WB Tools entry '%s': no Editor Utility Widget set in DA_WBToolsMenu."), *Entry.Label.ToString());
				return;
			}
			Subsystem->SpawnAndRegisterTab(Widget);
			return;
		}

		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Entry.PythonScript.FilePath);
		if (Entry.PythonScript.FilePath.IsEmpty() || !IFileManager::Get().FileExists(*Script))
		{
			UE_LOG(LogWBToolsMenu, Warning, TEXT("WB Tools entry '%s': Python script '%s' not found."), *Entry.Label.ToString(), *Script);
			return;
		}
		if (GEditor)
		{
			// The py console command comes from the Python Editor Script Plugin; without it the editor logs an unknown command.
			GEditor->Exec(nullptr, *FString::Printf(TEXT("py \"%s\""), *Script));
		}
	}

	void AddEntryFromData(UToolMenu* Menu, const FWB_ToolsMenuEntry& Entry, int32 Index)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection(Entry.Section, FText::FromName(Entry.Section));
		Section.AddMenuEntry(
			FName(*FString::Printf(TEXT("WBToolsData_%d"), Index)),
			Entry.Label,
			Entry.ToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Entry]() { RunEntry(Entry); })));
	}

	void FillMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Level = Menu->AddSection(TEXT("Level"), LOCTEXT("Level", "Level"));
		Level.AddMenuEntry(TEXT("CreateLevel"),
			LOCTEXT("CreateLevel", "Create Level"),
			LOCTEXT("CreateLevelTip", "Creates a level ready for a match from the template (Lv_Template): code of 3 digits (official) or 3 letters (gym), and a name."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::LevelCreatorTabId)));
		Level.AddMenuEntry(TEXT("SortLevel"),
			LOCTEXT("SortLevel", "Sort Level"),
			LOCTEXT("SortLevelTip", "Puts every actor of the open map in its Outliner folder and sub-level (rules in Project Settings > Editor > WB Tools)."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.WorldSettings")),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::SortLevelTabId)));

		FToolMenuSection& Content = Menu->AddSection(TEXT("Content"), LOCTEXT("Content", "Content"));
		Content.AddMenuEntry(TEXT("Collections"),
			LOCTEXT("Collections", "Collections"),
			LOCTEXT("CollectionsTip", "Builds your local Content Browser collections from the collection tags (Collection.*) of the Blueprints, Data Assets and maps."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::CollectionsTabId)));

		FToolMenuSection& Animation = Menu->AddSection(TEXT("Animation"), LOCTEXT("Animation", "Animation"));
		Animation.AddMenuEntry(TEXT("Retarget"),
			LOCTEXT("Retarget", "Retarget"),
			LOCTEXT("RetargetTip", "Copies animations from one character to another: IK Rigs, retargeter, chain mapping, batch retarget and the touch-ups of the target character."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::RetargetTabId)));

		FToolMenuSection& Gym = Menu->AddSection(TEXT("Gym"), LOCTEXT("Gym", "Gym"));
		Gym.AddMenuEntry(TEXT("GymSelector"),
			LOCTEXT("GymSelector", "Gym Mode"),
			LOCTEXT("GymSelectorTip", "Turns the open map into a gym and chooses the gym used by the next Play."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::GymTabId)));

		FToolMenuSection& Debug = Menu->AddSection(TEXT("Debug"), LOCTEXT("Debug", "Debug"));
		Debug.AddMenuEntry(TEXT("DebugKeys"),
			LOCTEXT("DebugKeys", "Debug Keys & Commands"),
			LOCTEXT("DebugKeysTip", "Debug keys (keyboard shortcuts of the Blueprints) and debug commands (WB.* console commands), read from the project."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FWB_ToolsMenu::OpenTab, FWB_ToolsMenu::DebugKeysTabId)));

		// Extra entries, read at each opening of the menu: a new Editor Utility Widget needs no recompile.
		const UWB_ToolsMenuData* Data = GetDefault<UWB_ToolsDeveloperSettings>()->MenuData.LoadSynchronous();
		if (!Data)
		{
			UE_LOG(LogWBToolsMenu, Warning, TEXT("WB Tools: Menu Data is not set (Project Settings > Editor > WB Tools): only the built-in tools are shown."));
			return;
		}
		for (int32 Index = 0; Index < Data->Entries.Num(); ++Index)
		{
			AddEntryFromData(Menu, Data->Entries[Index], Index);
		}
	}

	void ExtendMainMenu()
	{
		FToolMenuOwnerScoped OwnerScoped(OwnerName);
		UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu"));
		FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);
		FToolMenuEntry& Entry = Section.AddSubMenu(TEXT("WBTools"),
			LOCTEXT("WBTools", "WB Tools"),
			LOCTEXT("WBToolsTip", "Wrecking Ball tools for the team: level sorting, gyms, debug keys..."),
			FNewToolMenuDelegate::CreateStatic(&FillMenu));
		Entry.InsertPosition = FToolMenuInsert(TEXT("Help"), EToolMenuInsertType::Before);
	}
}

void FWB_ToolsMenu::Register()
{
	using namespace WBToolsMenu;

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SortLevelTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_SortLevelPanel>))
		.SetDisplayName(LOCTEXT("SortLevelTab", "Sort Level"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GymTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_GymSelectorPanel>))
		.SetDisplayName(LOCTEXT("GymTab", "Gym Mode"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DebugKeysTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_DebugKeysPanel>))
		.SetDisplayName(LOCTEXT("DebugKeysTab", "Debug Keys & Commands"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CollectionsTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_CollectionsPanel>))
		.SetDisplayName(LOCTEXT("CollectionsTab", "Collections"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RetargetTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_RetargetPanel>))
		.SetDisplayName(LOCTEXT("RetargetTab", "Retarget"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LevelCreatorTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTab<SWB_LevelCreatorPanel>))
		.SetDisplayName(LOCTEXT("LevelCreatorTab", "Create Level"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&ExtendMainMenu));
}

void FWB_ToolsMenu::Unregister()
{
	using namespace WBToolsMenu;

	UToolMenus::UnRegisterStartupCallback(StartupHandle);
	UToolMenus::UnregisterOwner(OwnerName);
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SortLevelTabId);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GymTabId);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebugKeysTabId);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CollectionsTabId);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RetargetTabId);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LevelCreatorTabId);
	}
}

void FWB_ToolsMenu::OpenTab(FName TabId)
{
	FGlobalTabmanager::Get()->TryInvokeTab(TabId);
}

#undef LOCTEXT_NAMESPACE
