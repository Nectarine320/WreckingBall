// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Collections/WB_CollectionsUI.h"

#include "ContentBrowserMenuContexts.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Framework/Application/SlateApplication.h"
#include "SGameplayTagPicker.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Tools/Collections/WB_CollectionsLibrary.h"
#include "Tools/WB_ToolsSettings.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WBCollectionsUI"

namespace WBCollectionsUI
{
	const FName OwnerName(TEXT("WreckingBallEditor.WBCollections"));
	FDelegateHandle StartupHandle;

	/** Blueprints and Data Assets only: the assets the Collections tool sorts. */
	TArray<UObject*> Taggable(const TArray<UObject*>& Objects)
	{
		TArray<UObject*> Result;
		for (UObject* Object : Objects)
		{
			if (Object && (Object->IsA<UBlueprint>() || Object->IsA<UDataAsset>()))
			{
				Result.Add(Object);
			}
		}
		return Result;
	}

	FText TagsText(const FGameplayTagContainer& Tags)
	{
		return Tags.IsEmpty() ? LOCTEXT("NoTag", "none") : FText::FromString(Tags.ToStringSimple());
	}

	void ExtendMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(OwnerName);

		// Toolbar of every asset editor (Blueprint, Data Asset...): shown only when the edited asset can be tagged.
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.DefaultToolBar"));
		Toolbar->FindOrAddSection(TEXT("Asset")).AddDynamicEntry(TEXT("WBCollections"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
		{
			const UAssetEditorToolkitMenuContext* Context = Section.FindContext<UAssetEditorToolkitMenuContext>();
			const TArray<UObject*> Assets = Context ? Taggable(Context->GetEditingObjects()) : TArray<UObject*>();
			if (Assets.Num() == 0)
			{
				return;
			}
			TArray<TWeakObjectPtr<UObject>> WeakAssets;
			for (UObject* Asset : Assets)
			{
				WeakAssets.Add(Asset);
			}
			Section.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("WBCollections"),
				FUIAction(FExecuteAction::CreateLambda([WeakAssets]()
				{
					TArray<UObject*> Alive;
					for (const TWeakObjectPtr<UObject>& Weak : WeakAssets)
					{
						if (UObject* Object = Weak.Get())
						{
							Alive.Add(Object);
						}
					}
					FWB_CollectionsUI::OpenTagPicker(Alive);
				})),
				LOCTEXT("CollectionsButton", "Collections"),
				TAttribute<FText>::CreateLambda([WeakAssets]()
				{
					UObject* Asset = WeakAssets.Num() > 0 ? WeakAssets[0].Get() : nullptr;
					return Asset
						? FText::Format(LOCTEXT("CollectionsTip", "Collection tags of this asset: {0}. Click to change them (WB Tools > Content > Collections)."), TagsText(UWB_CollectionsLibrary::GetCollectionTags(Asset)))
						: FText::GetEmpty();
				}),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ContentBrowser.AssetTreeFolderClosed"))));
		}));

		// Content Browser asset menu: several assets at once.
		UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu"));
		AssetMenu->FindOrAddSection(TEXT("GetAssetActions")).AddDynamicEntry(TEXT("WBCollectionTags"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
		{
			const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context)
			{
				return;
			}
			bool bAnyTaggable = false;
			for (const FAssetData& Asset : Context->SelectedAssets)
			{
				const UClass* Class = Asset.GetClass();
				bAnyTaggable |= Class && (Class->IsChildOf(UBlueprint::StaticClass()) || Class->IsChildOf(UDataAsset::StaticClass()));
			}
			if (!bAnyTaggable)
			{
				return;
			}
			const TArray<FAssetData> Selected = Context->SelectedAssets;
			Section.AddMenuEntry(TEXT("WBCollectionTags"),
				LOCTEXT("CollectionTagsEntry", "Collection Tags..."),
				LOCTEXT("CollectionTagsEntryTip", "Set the collection tags (Collection.*) of the selected Blueprints and Data Assets. WB Tools > Content > Collections builds the collections from them."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ContentBrowser.AssetTreeFolderClosed")),
				FUIAction(FExecuteAction::CreateLambda([Selected]()
				{
					TArray<UObject*> Objects;
					for (const FAssetData& Asset : Selected)
					{
						Objects.Add(Asset.GetAsset());
					}
					FWB_CollectionsUI::OpenTagPicker(Taggable(Objects));
				})));
		}));
	}
}

void FWB_CollectionsUI::Register()
{
	WBCollectionsUI::StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&WBCollectionsUI::ExtendMenus));
}

void FWB_CollectionsUI::Unregister()
{
	UToolMenus::UnRegisterStartupCallback(WBCollectionsUI::StartupHandle);
	UToolMenus::UnregisterOwner(WBCollectionsUI::OwnerName);
}

void FWB_CollectionsUI::OpenTagPicker(const TArray<UObject*>& Assets)
{
	if (Assets.Num() == 0)
	{
		return;
	}
	const FGameplayTag Root = GetDefault<UWB_ToolsDeveloperSettings>()->CollectionRootTag;
	const FGameplayTagContainer Current = UWB_CollectionsLibrary::GetCollectionTags(Assets[0]);
	FString Sources;
	const FGameplayTagContainer Inherited = UWB_CollectionsLibrary::GetInheritedCollectionTags(Assets[0], Sources);

	TArray<FString> Names;
	for (const UObject* Asset : Assets)
	{
		Names.Add(Asset->GetName());
	}
	TArray<TWeakObjectPtr<UObject>> WeakAssets;
	for (UObject* Asset : Assets)
	{
		WeakAssets.Add(Asset);
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("PickerTitle", "Collection Tags - {0}"), FText::FromString(FString::Join(Names, TEXT(", ")))))
		.ClientSize(FVector2D(460.0f, 560.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(Assets.Num() > 1
				? FText::Format(LOCTEXT("PickerHelpMany", "{0} assets: the tags checked here replace the tags of all of them. Several tags are allowed. Save the assets after."), Assets.Num())
				: LOCTEXT("PickerHelp", "Check the collections of this asset (several allowed). An asset without its own tag inherits the tags of its class. Save the asset after."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(WBToolsStyle::Dim)
			.Text(Inherited.IsEmpty()
				? LOCTEXT("NoInherited", "Inherited: none.")
				: FText::Format(LOCTEXT("Inherited", "Inherited: {0}. An own tag replaces the inherited one of the same master tag (Design, Environment...)."), FText::FromString(Sources)))
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SGameplayTagPicker)
			.Filter(Root.IsValid() ? Root.ToString() : FString())
			.MultiSelect(true)
			.GameplayTagPickerMode(EGameplayTagPickerMode::SelectionMode)
			.TagContainers({ Current })
			.OnTagChanged_Lambda([WeakAssets](const TArray<FGameplayTagContainer>& Containers)
			{
				const FGameplayTagContainer Tags = Containers.Num() > 0 ? Containers[0] : FGameplayTagContainer();
				for (const TWeakObjectPtr<UObject>& Weak : WeakAssets)
				{
					if (UObject* Asset = Weak.Get())
					{
						UWB_CollectionsLibrary::SetCollectionTags(Asset, Tags);
					}
				}
			})
		]);

	FSlateApplication::Get().AddWindow(Window);
}

#undef LOCTEXT_NAMESPACE
