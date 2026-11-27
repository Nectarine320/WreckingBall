// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/DebugKeys/WB_DebugKeysLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "K2Node_InputKey.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_DebugKeysLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogWBDebugKeys, Log, All);

namespace WBDebugKeys
{
	/** Class name of the Input Debug Key node: it lives in a private header of Enhanced Input, so it is read by reflection. */
	const TCHAR* InputDebugKeyNodeName = TEXT("K2Node_InputDebugKey");

	bool Contains(const TArray<uint8>& Bytes, const FAnsiStringView Needle)
	{
		const int32 Last = Bytes.Num() - Needle.Len();
		for (int32 Index = 0; Index <= Last; ++Index)
		{
			if (Bytes[Index] == Needle[0] && FMemory::Memcmp(&Bytes[Index], Needle.GetData(), Needle.Len()) == 0)
			{
				return true;
			}
		}
		return false;
	}

	/** True when the Blueprint may hold a keyboard event node. Unloaded assets are read from disk (name table) without loading them. */
	bool MayHoldKeyNode(const FAssetData& Asset)
	{
		if (Asset.IsAssetLoaded())
		{
			return true;
		}
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(Asset.PackageName.ToString(), Filename, FPackageName::GetAssetPackageExtension()))
		{
			return false;
		}
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Filename, FILEREAD_Silent))
		{
			return false;
		}
		return Contains(Bytes, "K2Node_InputKey") || Contains(Bytes, "K2Node_InputDebugKey");
	}

	FKey ReadKey(const UEdGraphNode* Node, const TCHAR* PropertyName)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(Node->GetClass(), PropertyName);
		return Property && Property->Struct == FKey::StaticStruct() ? *Property->ContainerPtrToValuePtr<FKey>(Node) : FKey();
	}

	bool ReadBool(const UEdGraphNode* Node, const TCHAR* PropertyName)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Node->GetClass(), PropertyName);
		return Property && Property->GetPropertyValue_InContainer(Node);
	}

	/** Key of a keyboard event node with its modifiers ("Ctrl+Shift+F7"). Empty when the node is not a keyboard event node. */
	FString ReadKeyText(const UEdGraphNode* Node, FKey& OutKey)
	{
		const FString ClassName = Node->GetClass()->GetName();
		const bool bDebugKey = ClassName == InputDebugKeyNodeName;
		if (!bDebugKey && !Node->IsA<UK2Node_InputKey>())
		{
			return FString();
		}
		OutKey = bDebugKey ? ReadKey(Node, TEXT("DebugKey")) : FKey();
		if (!OutKey.IsValid())
		{
			OutKey = ReadKey(Node, TEXT("InputKey"));
		}
		if (!OutKey.IsValid())
		{
			return FString();
		}
		FString Text;
		Text += ReadBool(Node, TEXT("bControl")) ? TEXT("Ctrl+") : TEXT("");
		Text += ReadBool(Node, TEXT("bAlt")) ? TEXT("Alt+") : TEXT("");
		Text += ReadBool(Node, TEXT("bShift")) ? TEXT("Shift+") : TEXT("");
		Text += ReadBool(Node, TEXT("bCommand")) ? TEXT("Cmd+") : TEXT("");
		Text += OutKey.GetDisplayName(/*bLongDisplayName=*/false).ToString();
		return Text;
	}

	/** Title of the smallest Comment box around the node, empty when none. */
	FString ReadCommentBoxTitle(const UEdGraph* Graph, const UEdGraphNode* Node)
	{
		const UEdGraphNode_Comment* Best = nullptr;
		int64 BestArea = MAX_int64;
		for (const UEdGraphNode* Other : Graph->Nodes)
		{
			const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Other);
			if (!Comment)
			{
				continue;
			}
			const bool bInside = Node->NodePosX >= Comment->NodePosX && Node->NodePosY >= Comment->NodePosY
				&& Node->NodePosX <= Comment->NodePosX + Comment->NodeWidth && Node->NodePosY <= Comment->NodePosY + Comment->NodeHeight;
			const int64 Area = int64(Comment->NodeWidth) * int64(Comment->NodeHeight);
			if (bInside && Area < BestArea)
			{
				Best = Comment;
				BestArea = Area;
			}
		}
		return Best ? Best->NodeComment.TrimStartAndEnd() : FString();
	}

	bool IsKeyboardKey(const FKey& Key)
	{
		return Key.IsValid() && !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsTouch() && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
	}

	TArray<FAssetData> FindAssets(const UClass* Class)
	{
		TArray<FAssetData> Assets;
		const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssetsByClass(Class->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
		Assets.RemoveAll([](const FAssetData& Asset) { return !Asset.PackagePath.ToString().StartsWith(TEXT("/Game")); });
		return Assets;
	}

	UBlueprint* LoadBlueprint(const FString& Asset)
	{
		return Cast<UBlueprint>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Asset, *FPackageName::GetShortName(Asset))).TryLoad());
	}

	UEdGraphNode* FindNode(UBlueprint* Blueprint, const FGuid& NodeGuid)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph ? Graph->Nodes : TArray<TObjectPtr<UEdGraphNode>>())
			{
				if (Node && Node->NodeGuid == NodeGuid)
				{
					return Node;
				}
			}
		}
		return nullptr;
	}
}

bool UWB_DebugKeysLibrary::SetDebugKeyDescription(const FString& Asset, FGuid NodeGuid, const FString& Description)
{
	UBlueprint* Blueprint = WBDebugKeys::LoadBlueprint(Asset);
	UEdGraphNode* Node = Blueprint ? WBDebugKeys::FindNode(Blueprint, NodeGuid) : nullptr;
	if (!Node)
	{
		UE_LOG(LogWBDebugKeys, Warning, TEXT("Debug key node not found in %s: scan again (Refresh)."), *Asset);
		return false;
	}
	const FString Text = Description.TrimStartAndEnd();
	const FScopedTransaction Transaction(NSLOCTEXT("WBDebugKeys", "SetDescription", "Debug Key Description"));
	Node->Modify();
	Node->NodeComment = Text;
	Node->bCommentBubbleVisible = !Text.IsEmpty();
	Node->bCommentBubblePinned = !Text.IsEmpty();
	Blueprint->MarkPackageDirty();
	UE_LOG(LogWBDebugKeys, Log, TEXT("Debug key description written in %s: \"%s\". Save the Blueprint."), *Blueprint->GetName(), *Text);
	return true;
}

bool UWB_DebugKeysLibrary::OpenDebugKey(const FString& Asset, FGuid NodeGuid)
{
	UBlueprint* Blueprint = WBDebugKeys::LoadBlueprint(Asset);
	UEdGraphNode* Node = Blueprint ? WBDebugKeys::FindNode(Blueprint, NodeGuid) : nullptr;
	if (!Node)
	{
		UE_LOG(LogWBDebugKeys, Warning, TEXT("Debug key node not found in %s: scan again (Refresh)."), *Asset);
		return false;
	}
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
	return true;
}

bool UWB_DebugKeysLibrary::BrowseToDebugKey(const FString& Asset)
{
	UBlueprint* Blueprint = WBDebugKeys::LoadBlueprint(Asset);
	if (!Blueprint || !GEditor)
	{
		return false;
	}
	TArray<UObject*> Objects = { Blueprint };
	GEditor->SyncBrowserToObjects(Objects);
	return true;
}

TArray<FWB_DebugKeyEntry> UWB_DebugKeysLibrary::ScanDebugKeys()
{
	using namespace WBDebugKeys;

	const double StartTime = FPlatformTime::Seconds();
	TArray<FWB_DebugKeyEntry> Entries;
	TArray<FKey> EntryKeys;

	// ---------------------------------------------------------------- Blueprint keyboard event nodes
	const TArray<FAssetData> Blueprints = FindAssets(UBlueprint::StaticClass());
	int32 NumLoaded = 0;
	for (const FAssetData& Asset : Blueprints)
	{
		if (!MayHoldKeyNode(Asset))
		{
			continue;
		}
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		if (!Blueprint)
		{
			continue;
		}
		++NumLoaded;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				FKey Key;
				const FString KeyText = Node ? ReadKeyText(Node, Key) : FString();
				// Only real keyboard keys: Any Key, mouse and gamepad events are not debug shortcuts.
				if (KeyText.IsEmpty() || !Node->IsNodeEnabled() || Key == EKeys::AnyKey || !IsKeyboardKey(Key))
				{
					continue;
				}
				FWB_DebugKeyEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.Key = KeyText;
				Entry.Description = Node->NodeComment.TrimStartAndEnd();
				Entry.CommentBoxTitle = ReadCommentBoxTitle(Graph, Node);
				Entry.NodeGuid = Node->NodeGuid;
				Entry.Asset = Asset.PackageName.ToString();
				Entry.Graph = Graph->GetName();
				EntryKeys.Add(Key);
			}
		}
	}

	// ---------------------------------------------------------------- Keys taken by the game (Input Mapping Contexts)
	TMap<FKey, TArray<FString>> GameKeys;
	for (const FAssetData& Asset : FindAssets(UInputMappingContext::StaticClass()))
	{
		const UInputMappingContext* Context = Cast<UInputMappingContext>(Asset.GetAsset());
		if (!Context)
		{
			continue;
		}
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (IsKeyboardKey(Mapping.Key))
			{
				GameKeys.FindOrAdd(Mapping.Key).AddUnique(FString::Printf(TEXT("%s (%s)"), Mapping.Action ? *Mapping.Action->GetName() : TEXT("no action"), *Context->GetName()));
			}
		}
	}

	// ---------------------------------------------------------------- Conflicts
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		TArray<FString> Others;
		for (int32 OtherIndex = 0; OtherIndex < Entries.Num(); ++OtherIndex)
		{
			if (OtherIndex != Index && Entries[OtherIndex].Key == Entries[Index].Key)
			{
				Others.AddUnique(FString::Printf(TEXT("%s (%s)"), *FPackageName::GetShortName(Entries[OtherIndex].Asset), *Entries[OtherIndex].Graph));
			}
		}
		if (const TArray<FString>* Game = GameKeys.Find(EntryKeys[Index]))
		{
			for (const FString& Use : *Game)
			{
				Others.AddUnique(TEXT("game: ") + Use);
			}
		}
		Entries[Index].Conflict = FString::Join(Others, TEXT(", "));
	}

	Entries.Sort([](const FWB_DebugKeyEntry& A, const FWB_DebugKeyEntry& B)
	{
		return A.Key != B.Key ? A.Key < B.Key : A.Asset < B.Asset;
	});

	// ---------------------------------------------------------------- Console commands of the project
	TArray<FWB_DebugKeyEntry> Commands;
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([&Commands](const TCHAR* Name, IConsoleObject* Object)
	{
		FWB_DebugKeyEntry& Entry = Commands.AddDefaulted_GetRef();
		Entry.bConsoleCommand = true;
		Entry.Key = Name;
		Entry.Description = Object ? FString(Object->GetHelp()).TrimStartAndEnd() : FString();
	}), TEXT("WB."));
	Commands.Sort([](const FWB_DebugKeyEntry& A, const FWB_DebugKeyEntry& B) { return A.Key < B.Key; });
	Entries.Append(Commands);

	UE_LOG(LogWBDebugKeys, Log, TEXT("Debug keys: %d key node(s) in %d Blueprint(s) (%d scanned), %d console command(s), %.2f s."),
		Entries.Num() - Commands.Num(), NumLoaded, Blueprints.Num(), Commands.Num(), FPlatformTime::Seconds() - StartTime);
	return Entries;
}

FString UWB_DebugKeysLibrary::ToMarkdown(const TArray<FWB_DebugKeyEntry>& Entries)
{
	auto Cell = [](const FString& Text) { return Text.Replace(TEXT("|"), TEXT("\\|")).Replace(TEXT("\n"), TEXT(" ")); };

	TArray<FString> Lines;
	Lines.Add(TEXT("| Debug key | Description | Where | Conflict |"));
	Lines.Add(TEXT("|---|---|---|---|"));
	for (const FWB_DebugKeyEntry& Entry : Entries)
	{
		if (!Entry.bConsoleCommand)
		{
			Lines.Add(FString::Printf(TEXT("| %s | %s | %s (%s) | %s |"), *Cell(Entry.Key), Entry.Description.IsEmpty() ? TEXT("(no description)") : *Cell(Entry.Description),
				*FPackageName::GetShortName(Entry.Asset), *Entry.Graph, *Cell(Entry.Conflict)));
		}
	}
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("| Debug command | Help |"));
	Lines.Add(TEXT("|---|---|"));
	for (const FWB_DebugKeyEntry& Entry : Entries)
	{
		if (Entry.bConsoleCommand)
		{
			Lines.Add(FString::Printf(TEXT("| %s | %s |"), *Cell(Entry.Key), Entry.Description.IsEmpty() ? TEXT("(no description)") : *Cell(Entry.Description)));
		}
	}
	return FString::Join(Lines, TEXT("\n"));
}
