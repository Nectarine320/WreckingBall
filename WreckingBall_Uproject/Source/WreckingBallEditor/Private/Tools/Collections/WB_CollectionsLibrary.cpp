// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/Collections/WB_CollectionsLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "CollectionManagerModule.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"
#include "ICollectionContainer.h"
#include "ICollectionManager.h"
#include "Misc/PackageName.h"
#include "Tools/Gym/WB_GymSelectorLibrary.h"
#include "Tools/WB_ToolsSettings.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_CollectionsLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogWBCollections, Log, All);

namespace WBCollections
{
	/** Key of the tags: package metadata of the asset, Asset Registry tag, and C++ class meta. */
	const FName TagKey(TEXT("WBCollection"));
	FDelegateHandle ExtraTagsHandle;

	IAssetRegistry& Registry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	ICollectionContainer& Collections()
	{
		return *FModuleManager::LoadModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager")).Get().GetProjectCollectionContainer();
	}

	/** "Collection.Design.AI,Collection.Environment.Props" -> tags. Unknown tags are reported. */
	FGameplayTagContainer ParseTags(const FString& Text, const FString& Owner, TArray<FString>* Warnings)
	{
		FGameplayTagContainer Tags;
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), /*bCullEmpty=*/true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Part), /*ErrorIfNotFound=*/false);
			if (Tag.IsValid())
			{
				Tags.AddTag(Tag);
			}
			else if (Warnings && !Part.IsEmpty())
			{
				Warnings->AddUnique(FString::Printf(TEXT("%s: unknown collection tag '%s' (add it in Project Settings > Gameplay Tags)."), *Owner, *Part));
			}
		}
		return Tags;
	}

	FString TagsToString(const FGameplayTagContainer& Tags)
	{
		TArray<FString> Names;
		for (const FGameplayTag& Tag : Tags)
		{
			Names.Add(Tag.ToString());
		}
		return FString::Join(Names, TEXT(","));
	}

	/** A Blueprint class or its generated class both mean the Blueprint asset. */
	UObject* AssetOf(UObject* Object)
	{
		if (const UClass* Class = Cast<UClass>(Object))
		{
			return Class->ClassGeneratedBy ? Class->ClassGeneratedBy.Get() : Object;
		}
		return Object;
	}

	FString ReadOwnText(const UObject* Asset)
	{
		const UPackage* Package = Asset ? Asset->GetPackage() : nullptr;
		if (!Package)
		{
			return FString();
		}
		FMetaData& MetaData = const_cast<UPackage*>(Package)->GetMetaData();
		return MetaData.HasValue(Asset, TagKey) ? MetaData.GetValue(Asset, TagKey) : FString();
	}

	/** Own tags of an asset: from the loaded object when loaded (may be unsaved), else from the Asset Registry. */
	FString OwnText(const FAssetData& Asset)
	{
		if (Asset.IsAssetLoaded())
		{
			if (const UObject* Object = Asset.FastGetAsset(/*bLoad=*/false))
			{
				return ReadOwnText(Object);
			}
		}
		FString Value;
		Asset.GetTagValue(TagKey, Value);
		return Value;
	}

	FString ParentClassOf(const FAssetData& Blueprint)
	{
		FString Parent;
		Blueprint.GetTagValue(FBlueprintTags::ParentClassPath, Parent);
		return FPackageName::ExportTextPathToObjectPath(Parent);
	}

	FString GeneratedClassOf(const FAssetData& Blueprint)
	{
		FString Generated;
		Blueprint.GetTagValue(FBlueprintTags::GeneratedClassPath, Generated);
		return FPackageName::ExportTextPathToObjectPath(Generated);
	}

	/** Tags of one level of the class chain (an asset or a class) and its name, for the "inherited from" text. */
	struct FLevelTags
	{
		FGameplayTagContainer Tags;
		FString Source;
	};

	/** Walks a class and its parents: Blueprint classes through their Blueprint asset (Asset Registry), native classes through their meta. */
	void CollectClassChain(FString ClassPath, TArray<FLevelTags>& OutLevels, TArray<FString>* Warnings)
	{
		for (int32 Guard = 0; Guard < 64 && !ClassPath.IsEmpty(); ++Guard)
		{
			if (ClassPath.StartsWith(TEXT("/Script/")))
			{
				for (const UClass* Class = FindObject<UClass>(nullptr, *ClassPath); Class; Class = Class->GetSuperClass())
				{
					if (const FString* Meta = Class->FindMetaData(TagKey))
					{
						OutLevels.Add({ ParseTags(*Meta, Class->GetName(), Warnings), Class->GetName() });
					}
				}
				return;
			}
			const FString PackageName = FPackageName::ObjectPathToPackageName(ClassPath);
			const FString AssetName = FPackageName::GetShortName(PackageName);
			const FAssetData Blueprint = Registry().GetAssetByObjectPath(FSoftObjectPath(PackageName + TEXT(".") + AssetName));
			if (!Blueprint.IsValid())
			{
				return;
			}
			const FString Text = OwnText(Blueprint);
			if (!Text.IsEmpty())
			{
				OutLevels.Add({ ParseTags(Text, AssetName, Warnings), AssetName });
			}
			ClassPath = ParentClassOf(Blueprint);
		}
	}

	/** Master tag of a tag (its ancestor that is a direct child of the root), invalid when the tag is not under the root. */
	FGameplayTag MasterOf(const FGameplayTag& Tag, const FGameplayTag& Root)
	{
		for (FGameplayTag Current = Tag; Current.IsValid(); )
		{
			if (Current == Root)
			{
				return FGameplayTag();
			}
			const FGameplayTag Parent = Current.RequestDirectParent();
			if (Parent == Root)
			{
				return Current;
			}
			Current = Parent;
		}
		return FGameplayTag();
	}

	/** For each master tag, the tags of the closest level that has one in that branch. */
	FGameplayTagContainer Resolve(const TArray<FLevelTags>& Levels, const FGameplayTag& Root, FString* OutSources = nullptr)
	{
		FGameplayTagContainer Result;
		TSet<FGameplayTag> Claimed;
		TArray<FString> Sources;
		for (const FLevelTags& Level : Levels)
		{
			TSet<FGameplayTag> MastersHere;
			for (const FGameplayTag& Tag : Level.Tags)
			{
				const FGameplayTag Master = MasterOf(Tag, Root);
				if (Master.IsValid() && !Claimed.Contains(Master))
				{
					Result.AddTag(Tag);
					MastersHere.Add(Master);
					Sources.AddUnique(FString::Printf(TEXT("%s (from %s)"), *Tag.ToString(), *Level.Source));
				}
			}
			Claimed.Append(MastersHere);
		}
		if (OutSources)
		{
			*OutSources = FString::Join(Sources, TEXT(", "));
		}
		return Result;
	}

	int32 TagDepth(const FGameplayTag& Tag)
	{
		int32 Depth = 0;
		for (const TCHAR Char : Tag.ToString())
		{
			Depth += Char == TEXT('.') ? 1 : 0;
		}
		return Depth;
	}

	/** Color of a collection: the color of its master tag, darker by ShadeStep for each level below the master. Unset when the master has no color. */
	TOptional<FLinearColor> ColorOf(const FGameplayTag& Tag, int32 ExtraLevels, const FGameplayTag& Root)
	{
		const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
		const FGameplayTag Master = MasterOf(Tag, Root);
		const FLinearColor* Base = Master.IsValid() ? Settings->MasterCollectionColors.Find(Master) : nullptr;
		if (!Base)
		{
			return TOptional<FLinearColor>();
		}
		const int32 Levels = TagDepth(Tag) - TagDepth(Master) + ExtraLevels;
		const float Factor = FMath::Pow(1.0f - FMath::Clamp(Settings->CollectionShadeStep, 0.0f, 1.0f), static_cast<float>(Levels));
		// Darken the color as seen on screen (sRGB), so that each step is visible.
		const FColor Screen = Base->ToFColorSRGB();
		const FColor Darker(FMath::RoundToInt(Screen.R * Factor), FMath::RoundToInt(Screen.G * Factor), FMath::RoundToInt(Screen.B * Factor), 255);
		return FLinearColor::FromSRGBColor(Darker);
	}

	void OnGetExtraObjectTags(FAssetRegistryTagsContext Context)
	{
		// Package metadata is read on the game thread only (concurrent saves run elsewhere).
		const UObject* Object = Context.GetObject();
		if (!IsInGameThread() || !Object || !Object->IsAsset())
		{
			return;
		}
		const FString Text = ReadOwnText(Object);
		if (!Text.IsEmpty())
		{
			Context.AddTag(UObject::FAssetRegistryTag(TagKey, Text, UObject::FAssetRegistryTag::TT_Alphabetical));
		}
	}
}

void UWB_CollectionsLibrary::RegisterAssetRegistryTags()
{
	WBCollections::ExtraTagsHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddStatic(&WBCollections::OnGetExtraObjectTags);
}

void UWB_CollectionsLibrary::UnregisterAssetRegistryTags()
{
	UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(WBCollections::ExtraTagsHandle);
}

TArray<FGameplayTag> UWB_CollectionsLibrary::GetMasterTags()
{
	TArray<FGameplayTag> Masters;
	const FGameplayTag Root = GetDefault<UWB_ToolsDeveloperSettings>()->CollectionRootTag;
	if (!Root.IsValid())
	{
		UE_LOG(LogWBCollections, Warning, TEXT("Collection Root Tag is not set in Project Settings > Editor > WB Tools > Collections."));
		return Masters;
	}
	for (const FGameplayTag& Tag : UGameplayTagsManager::Get().RequestGameplayTagChildren(Root))
	{
		if (Tag.RequestDirectParent() == Root)
		{
			Masters.Add(Tag);
		}
	}
	Masters.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	return Masters;
}

bool UWB_CollectionsLibrary::IsMasterTagEnabled(FGameplayTag MasterTag)
{
	return GetDefault<UWB_ToolsUserSettings>()->EnabledMasterTags.HasTagExact(MasterTag);
}

void UWB_CollectionsLibrary::SetMasterTagEnabled(FGameplayTag MasterTag, bool bEnabled)
{
	UWB_ToolsUserSettings* UserSettings = GetMutableDefault<UWB_ToolsUserSettings>();
	if (bEnabled)
	{
		UserSettings->EnabledMasterTags.AddTag(MasterTag);
	}
	else
	{
		UserSettings->EnabledMasterTags.RemoveTag(MasterTag);
	}
	UserSettings->SaveConfig();
}

FGameplayTagContainer UWB_CollectionsLibrary::GetCollectionTags(UObject* Asset)
{
	UObject* Target = WBCollections::AssetOf(Asset);
	return Target ? WBCollections::ParseTags(WBCollections::ReadOwnText(Target), Target->GetName(), nullptr) : FGameplayTagContainer();
}

bool UWB_CollectionsLibrary::SetCollectionTags(UObject* Asset, const FGameplayTagContainer& Tags)
{
	UObject* Target = WBCollections::AssetOf(Asset);
	UPackage* Package = Target ? Target->GetPackage() : nullptr;
	if (!Package || !Target->IsAsset())
	{
		UE_LOG(LogWBCollections, Warning, TEXT("SetCollectionTags: not an asset."));
		return false;
	}
	FMetaData& MetaData = Package->GetMetaData();
	if (Tags.IsEmpty())
	{
		MetaData.RemoveValue(Target, WBCollections::TagKey);
	}
	else
	{
		MetaData.SetValue(Target, WBCollections::TagKey, *WBCollections::TagsToString(Tags));
	}
	Target->MarkPackageDirty();
	WBCollections::Registry().AssetUpdateTags(Target, EAssetRegistryTagsCaller::FullUpdate);
	UE_LOG(LogWBCollections, Log, TEXT("%s collection tags: %s. Save the asset."), *Target->GetName(), Tags.IsEmpty() ? TEXT("none") : *Tags.ToStringSimple());
	return true;
}

FGameplayTagContainer UWB_CollectionsLibrary::GetInheritedCollectionTags(UObject* Asset, FString& OutSources)
{
	OutSources.Reset();
	UObject* Target = WBCollections::AssetOf(Asset);
	if (!Target)
	{
		return FGameplayTagContainer();
	}
	TArray<WBCollections::FLevelTags> Levels;
	const UBlueprint* Blueprint = Cast<UBlueprint>(Target);
	const UClass* StartClass = Blueprint ? Blueprint->ParentClass.Get() : Target->GetClass();
	if (StartClass)
	{
		WBCollections::CollectClassChain(StartClass->GetPathName(), Levels, nullptr);
	}
	return WBCollections::Resolve(Levels, GetDefault<UWB_ToolsDeveloperSettings>()->CollectionRootTag, &OutSources);
}

FWB_CollectionsReport UWB_CollectionsLibrary::PreviewCollections()
{
	return Run(false);
}

FWB_CollectionsReport UWB_CollectionsLibrary::UpdateCollections()
{
	return Run(true);
}

FWB_CollectionsReport UWB_CollectionsLibrary::Run(bool bApply)
{
	using namespace WBCollections;

	FWB_CollectionsReport Report;
	const UWB_ToolsDeveloperSettings* Settings = GetDefault<UWB_ToolsDeveloperSettings>();
	const FGameplayTag Root = Settings->CollectionRootTag;
	if (!Root.IsValid())
	{
		Report.Warnings.Add(TEXT("Collection Root Tag is not set in Project Settings > Editor > WB Tools > Collections."));
		return Report;
	}
	const FGameplayTagContainer& Enabled = GetDefault<UWB_ToolsUserSettings>()->EnabledMasterTags;
	const FName UntaggedName = Settings->UntaggedCollectionName.IsEmpty() ? NAME_None : FName(*Settings->UntaggedCollectionName);
	if (UntaggedName.IsNone())
	{
		Report.Warnings.Add(TEXT("Untagged Collection Name is not set in Project Settings > Editor > WB Tools: untagged Data Assets are only listed."));
	}

	// ---------------------------------------------------------------- Collection names (one per tag, unique)
	TMap<FGameplayTag, FName> NameOf;
	TSet<FName> UsedNames;
	TArray<FGameplayTag> AllTags;
	UGameplayTagsManager::Get().RequestGameplayTagChildren(Root).GetGameplayTagArray(AllTags);
	AllTags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return TagDepth(A) != TagDepth(B) ? TagDepth(A) < TagDepth(B) : A.ToString() < B.ToString(); });
	for (const FGameplayTag& Tag : AllTags)
	{
		FString Leaf = Tag.ToString();
		Leaf = Leaf.Mid(Leaf.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1);
		FName Name(*Leaf);
		if (UsedNames.Contains(Name) || Name == UntaggedName)
		{
			FString Parent = Tag.RequestDirectParent().ToString();
			Parent = Parent.Mid(Parent.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1);
			Name = FName(*FString::Printf(TEXT("%s_%s"), *Parent, *Leaf));
		}
		UsedNames.Add(Name);
		NameOf.Add(Tag, Name);
	}

	// ---------------------------------------------------------------- Expected content
	TMap<FName, TSet<FSoftObjectPath>> Expected;
	TMap<FName, FName> ParentOf;
	TMap<FName, int32> DepthOf;
	TMap<FName, TOptional<FLinearColor>> ColorOfName;
	auto IsEnabled = [&](const FGameplayTag& Tag)
	{
		const FGameplayTag Master = MasterOf(Tag, Root);
		return Master.IsValid() && Enabled.HasTagExact(Master);
	};
	auto Want = [&](const FGameplayTag& Tag) -> TSet<FSoftObjectPath>&
	{
		// The collection of the tag, and every parent collection up to the master tag.
		for (FGameplayTag Current = Tag; Current.IsValid() && Current != Root; Current = Current.RequestDirectParent())
		{
			const FName Name = NameOf.FindRef(Current);
			Expected.FindOrAdd(Name);
			ColorOfName.Add(Name, ColorOf(Current, 0, Root));
			DepthOf.Add(Name, TagDepth(Current));
			const FGameplayTag Parent = Current.RequestDirectParent();
			ParentOf.Add(Name, Parent.IsValid() && Parent != Root ? NameOf.FindRef(Parent) : NAME_None);
		}
		return Expected.FindChecked(NameOf.FindRef(Tag));
	};
	auto AddMember = [&](const FGameplayTagContainer& Tags, const FSoftObjectPath& Path)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (IsEnabled(Tag))
			{
				Want(Tag).Add(Path);
			}
		}
	};
	const bool bAnyEnabled = Enabled.Num() > 0;

	// Data Assets: every instance, tags from the asset, then its class chain.
	TArray<FAssetData> DataAssets;
	Registry().GetAssetsByClass(UDataAsset::StaticClass()->GetClassPathName(), DataAssets, /*bSearchSubClasses=*/true);
	TMap<FString, int32> UntaggedCount;
	for (const FAssetData& Asset : DataAssets)
	{
		if (!Asset.PackagePath.ToString().StartsWith(TEXT("/Game")))
		{
			continue;
		}
		TArray<FLevelTags> Levels;
		const FString Own = OwnText(Asset);
		if (!Own.IsEmpty())
		{
			Levels.Add({ ParseTags(Own, Asset.AssetName.ToString(), &Report.Warnings), Asset.AssetName.ToString() });
		}
		CollectClassChain(Asset.AssetClassPath.ToString(), Levels, &Report.Warnings);
		const FGameplayTagContainer Tags = Resolve(Levels, Root);
		if (Tags.IsEmpty())
		{
			// Only the classes of the project are expected to carry a tag: engine and plugin Data Assets (Input Actions...) are left out.
			const FString ClassPackage = Asset.AssetClassPath.GetPackageName().ToString();
			if (!ClassPackage.StartsWith(TEXT("/Game")) && !ClassPackage.StartsWith(TEXT("/Script/WreckingBall")))
			{
				continue;
			}
			++UntaggedCount.FindOrAdd(Asset.AssetClassPath.GetAssetName().ToString());
			if (bAnyEnabled && !UntaggedName.IsNone())
			{
				Expected.FindOrAdd(UntaggedName).Add(Asset.GetSoftObjectPath());
				ParentOf.Add(UntaggedName, NAME_None);
				DepthOf.Add(UntaggedName, 1);
			}
			continue;
		}
		AddMember(Tags, Asset.GetSoftObjectPath());
	}
	for (const TPair<FString, int32>& Pair : UntaggedCount)
	{
		Report.UntaggedClasses.Add(FString::Printf(TEXT("%s (%d)"), *Pair.Key, Pair.Value));
	}
	Report.UntaggedClasses.Sort();

	// Blueprints: actor Blueprints only when they are the last child of their branch (no Blueprint child, not *_Base).
	TArray<FAssetData> Blueprints;
	Registry().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Blueprints, /*bSearchSubClasses=*/true);
	TSet<FString> ParentClasses;
	for (const FAssetData& Blueprint : Blueprints)
	{
		ParentClasses.Add(ParentClassOf(Blueprint));
	}
	for (const FAssetData& Blueprint : Blueprints)
	{
		if (!Blueprint.PackagePath.ToString().StartsWith(TEXT("/Game")))
		{
			continue;
		}
		FString NativeParent;
		Blueprint.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParent);
		const UClass* Native = FindObject<UClass>(nullptr, *FPackageName::ExportTextPathToObjectPath(NativeParent));
		const bool bActor = Native && Native->IsChildOf(AActor::StaticClass());
		if (bActor && (ParentClasses.Contains(GeneratedClassOf(Blueprint)) || Blueprint.AssetName.ToString().EndsWith(TEXT("_Base"))))
		{
			continue;
		}
		TArray<FLevelTags> Levels;
		const FString Own = OwnText(Blueprint);
		if (!Own.IsEmpty())
		{
			Levels.Add({ ParseTags(Own, Blueprint.AssetName.ToString(), &Report.Warnings), Blueprint.AssetName.ToString() });
		}
		CollectClassChain(ParentClassOf(Blueprint), Levels, &Report.Warnings);
		AddMember(Resolve(Levels, Root), Blueprint.GetSoftObjectPath());
	}

	// Maps: Official, Gyms and Unsorted under the maps master tag, from the level name patterns.
	const FGameplayTag MapsTag = Settings->MapsCollectionTag;
	if (MapsTag.IsValid() && IsEnabled(MapsTag))
	{
		const FName MapsName = NameOf.FindRef(MapsTag);
		Want(MapsTag);
		const FName GroupNames[] = { TEXT("Official"), TEXT("Gyms"), TEXT("Templates"), TEXT("Unsorted") };
		for (const FString& MapPackage : UWB_GymSelectorLibrary::GetLevelList())
		{
			const int32 Group = static_cast<int32>(UWB_GymSelectorLibrary::GetLevelCategoryOfPackage(MapPackage));
			const FName Name = UsedNames.Contains(GroupNames[Group]) ? FName(*FString::Printf(TEXT("Maps_%s"), *GroupNames[Group].ToString())) : GroupNames[Group];
			Expected.FindOrAdd(Name).Add(FSoftObjectPath(MapPackage + TEXT(".") + FPackageName::GetShortName(MapPackage)));
			ParentOf.Add(Name, MapsName);
			DepthOf.Add(Name, TagDepth(MapsTag) + 1);
			ColorOfName.Add(Name, ColorOf(MapsTag, 1, Root));
		}
	}

	// ---------------------------------------------------------------- Compare with the local collections
	ICollectionContainer& Container = Collections();
	const ECollectionShareType::Type Local = ECollectionShareType::CST_Local;

	TArray<FName> Ordered;
	Expected.GetKeys(Ordered);
	Ordered.Sort([&DepthOf](const FName& A, const FName& B) { return DepthOf.FindRef(A) != DepthOf.FindRef(B) ? DepthOf.FindRef(A) < DepthOf.FindRef(B) : A.LexicalLess(B); });
	Report.NumCollections = Ordered.Num();

	// Collections of unchecked master tags: removed, deepest first.
	TArray<FName> ToDestroy;
	for (const FGameplayTag& Tag : AllTags)
	{
		const FName Name = NameOf.FindRef(Tag);
		const FGameplayTag Master = MasterOf(Tag, Root);
		if (Master.IsValid() && !Enabled.HasTagExact(Master) && !Expected.Contains(Name) && Container.CollectionExists(Name, Local))
		{
			ToDestroy.Add(Name);
		}
	}
	if (MapsTag.IsValid() && !IsEnabled(MapsTag))
	{
		for (const TCHAR* Group : { TEXT("Official"), TEXT("Gyms"), TEXT("Templates"), TEXT("Unsorted"), TEXT("Maps_Official"), TEXT("Maps_Gyms"), TEXT("Maps_Templates"), TEXT("Maps_Unsorted") })
		{
			if (!UsedNames.Contains(FName(Group)) && Container.CollectionExists(FName(Group), Local))
			{
				ToDestroy.AddUnique(FName(Group));
			}
		}
	}
	if (!bAnyEnabled && !UntaggedName.IsNone() && Container.CollectionExists(UntaggedName, Local))
	{
		ToDestroy.Add(UntaggedName);
	}

	for (const FName& Name : Ordered)
	{
		const TSet<FSoftObjectPath>& Wanted = Expected.FindChecked(Name);
		TArray<FSoftObjectPath> Current;
		if (!Container.CollectionExists(Name, Local))
		{
			Report.CreatedCollections.Add(Name.ToString());
		}
		else
		{
			Container.GetAssetsInCollection(Name, Local, Current, ECollectionRecursionFlags::Self);
		}
		const TSet<FSoftObjectPath> CurrentSet(Current);
		for (const FSoftObjectPath& Path : Wanted)
		{
			if (!CurrentSet.Contains(Path))
			{
				Report.Changes.Add({ Name.ToString(), Path.ToString(), true });
			}
		}
		for (const FSoftObjectPath& Path : CurrentSet)
		{
			if (!Wanted.Contains(Path))
			{
				Report.Changes.Add({ Name.ToString(), Path.ToString(), false });
			}
		}
	}
	for (const FName& Name : ToDestroy)
	{
		Report.RemovedCollections.Add(Name.ToString());
	}

	// ---------------------------------------------------------------- Apply
	if (bApply)
	{
		FText Error;
		for (const FName& Name : Ordered)
		{
			if (!Container.CollectionExists(Name, Local) && !Container.CreateCollection(Name, Local, ECollectionStorageMode::Static, &Error))
			{
				Report.Warnings.Add(FString::Printf(TEXT("Collection %s not created: %s"), *Name.ToString(), *Error.ToString()));
				continue;
			}
			const FName Parent = ParentOf.FindRef(Name);
			const TOptional<FCollectionNameType> CurrentParent = Container.GetParentCollection(Name, Local);
			const FName CurrentParentName = CurrentParent.IsSet() ? CurrentParent->Name : NAME_None;
			if (CurrentParentName != Parent)
			{
				Container.ReparentCollection(Name, Local, Parent, Parent.IsNone() ? ECollectionShareType::CST_All : Local, &Error);
			}
			Container.SetCollectionColor(Name, Local, ColorOfName.FindRef(Name));
		}
		TMap<FName, TArray<FSoftObjectPath>> Adds;
		TMap<FName, TArray<FSoftObjectPath>> Removes;
		for (const FWB_CollectionChange& Change : Report.Changes)
		{
			(Change.bAdded ? Adds : Removes).FindOrAdd(FName(*Change.Collection)).Add(FSoftObjectPath(Change.Asset));
		}
		for (const TPair<FName, TArray<FSoftObjectPath>>& Pair : Removes)
		{
			Container.RemoveFromCollection(Pair.Key, Local, Pair.Value);
		}
		for (const TPair<FName, TArray<FSoftObjectPath>>& Pair : Adds)
		{
			Container.AddToCollection(Pair.Key, Local, Pair.Value);
		}
		ToDestroy.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
		// Children before parents: destroy until nothing is left, a parent fails while it still has a child.
		for (int32 Pass = 0; Pass < 8 && ToDestroy.Num() > 0; ++Pass)
		{
			for (int32 Index = ToDestroy.Num() - 1; Index >= 0; --Index)
			{
				TArray<FName> Children;
				Container.GetChildCollectionNames(ToDestroy[Index], Local, Local, Children);
				if (Children.Num() == 0)
				{
					Container.DestroyCollection(ToDestroy[Index], Local);
					ToDestroy.RemoveAt(Index);
				}
			}
		}
		Report.bApplied = true;
	}

	UE_LOG(LogWBCollections, Log, TEXT("%s"), *ReportToText(Report));
	return Report;
}

FString UWB_CollectionsLibrary::ReportToText(const FWB_CollectionsReport& Report)
{
	int32 NumAdded = 0;
	for (const FWB_CollectionChange& Change : Report.Changes)
	{
		NumAdded += Change.bAdded ? 1 : 0;
	}
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Collections %s: %d collection(s), %d created, %d removed, %d asset(s) added, %d removed."),
		Report.bApplied ? TEXT("updated") : TEXT("preview"), Report.NumCollections, Report.CreatedCollections.Num(), Report.RemovedCollections.Num(),
		NumAdded, Report.Changes.Num() - NumAdded));
	for (const FString& Warning : Report.Warnings)
	{
		Lines.Add(TEXT("  WARNING: ") + Warning);
	}
	if (Report.UntaggedClasses.Num() > 0)
	{
		Lines.Add(TEXT("  Data Asset classes without collection tag: ") + FString::Join(Report.UntaggedClasses, TEXT(", ")));
	}
	for (const FWB_CollectionChange& Change : Report.Changes)
	{
		Lines.Add(FString::Printf(TEXT("  %s %s %s"), Change.bAdded ? TEXT("+") : TEXT("-"), *Change.Collection, *Change.Asset));
	}
	return FString::Join(Lines, TEXT("\n"));
}
