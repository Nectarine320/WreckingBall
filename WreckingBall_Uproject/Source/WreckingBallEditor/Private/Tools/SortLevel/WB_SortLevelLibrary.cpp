// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/SortLevel/WB_SortLevelLibrary.h"

#include "ActorEditorUtils.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Editor/GroupActor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Tools/SortLevel/WB_SortLevelRules.h"
#include "Tools/WB_ToolsSettings.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_SortLevelLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogWBSortLevel, Log, All);

#define LOCTEXT_NAMESPACE "WBSortLevel"

namespace WBSortLevel
{
	/** One actor to change, with its target folder and level. */
	struct FPlan
	{
		AActor* Actor = nullptr;
		FName Folder;
		ULevel* DestLevel = nullptr;
		FWB_SortLevelMove Move;
	};

	FString LevelDisplayName(const ULevel* Level)
	{
		if (!Level)
		{
			return TEXT("?");
		}
		const FString Name = FPackageName::GetShortName(Level->GetOutermost()->GetName());
		return Level->IsPersistentLevel() ? FString::Printf(TEXT("%s (persistent)"), *Name) : Name;
	}

	/** Number of steps from Class up to Parent, INDEX_NONE when Class is not a child of Parent. */
	int32 ClassDistance(const UClass* Class, const UClass* Parent)
	{
		int32 Distance = 0;
		for (const UClass* Current = Class; Current; Current = Current->GetSuperClass(), ++Distance)
		{
			if (Current == Parent)
			{
				return Distance;
			}
		}
		return INDEX_NONE;
	}

	/** Rule of the closest parent class of Class, null when none matches. */
	const FWB_SortClassRule* FindRule(const UWB_SortLevelRules* Rules, const UClass* Class)
	{
		const FWB_SortClassRule* Best = nullptr;
		int32 BestDistance = MAX_int32;
		for (const FWB_SortClassRule& Rule : Rules->ClassRules)
		{
			const UClass* RuleClass = Rule.ActorClass.LoadSynchronous();
			if (!RuleClass || Rule.Folder.IsNone())
			{
				continue;
			}
			const int32 Distance = ClassDistance(Class, RuleClass);
			if (Distance != INDEX_NONE && Distance < BestDistance)
			{
				Best = &Rule;
				BestDistance = Distance;
			}
		}
		return Best;
	}

	bool IsIgnored(const UWB_SortLevelRules* Rules, const UClass* Class)
	{
		for (const TSoftClassPtr<AActor>& Ignored : Rules->IgnoredClasses)
		{
			const UClass* IgnoredClass = Ignored.LoadSynchronous();
			if (IgnoredClass && Class->IsChildOf(IgnoredClass))
			{
				return true;
			}
		}
		return false;
	}

	/** Actors that the Outliner shows and that the tool may move. */
	bool IsSortable(AActor* Actor)
	{
		return IsValid(Actor)
			&& !Actor->IsA<AWorldSettings>()
			&& !Actor->IsA<ALevelScriptActor>()
			&& !FActorEditorUtils::IsABuilderBrush(Actor)
			&& !Actor->IsChildActor()
			&& !Actor->HasAnyFlags(RF_Transient)
			&& Actor->IsListedInSceneOutliner();
	}

	FName RootOf(FName Folder)
	{
		FString Text = Folder.ToString();
		int32 Slash = INDEX_NONE;
		if (Text.FindChar(TEXT('/'), Slash))
		{
			Text.LeftInline(Slash);
		}
		return FName(*Text);
	}

	/** True when Current is Target or one of its sub-folders. */
	bool IsInFolder(FName Current, FName Target)
	{
		return Current == Target || Current.ToString().StartsWith(Target.ToString() + TEXT("/"));
	}

	/** Loaded sub-level whose name ends with Suffix. Null and a warning when none, first one and a warning when several. */
	ULevel* FindSubLevel(UWorld* World, const FString& Suffix, TArray<FString>& OutWarnings)
	{
		TArray<ULevel*> Found;
		for (ULevelStreaming* Streaming : World->GetStreamingLevels())
		{
			ULevel* Level = Streaming ? Streaming->GetLoadedLevel() : nullptr;
			if (Level && FPackageName::GetShortName(Streaming->GetWorldAssetPackageName()).EndsWith(Suffix, ESearchCase::IgnoreCase))
			{
				Found.Add(Level);
			}
		}
		if (Found.Num() > 1)
		{
			OutWarnings.Add(FString::Printf(TEXT("Several loaded sub-levels end with '%s': %s is used."), *Suffix, *LevelDisplayName(Found[0])));
		}
		return Found.Num() > 0 ? Found[0] : nullptr;
	}

	void AddWarning(FWB_SortLevelMove& Move, const FString& Text)
	{
		Move.Warning = Move.Warning.IsEmpty() ? Text : Move.Warning + TEXT(" ") + Text;
	}
}

UWB_SortLevelRules* UWB_SortLevelLibrary::LoadRules()
{
	UWB_SortLevelRules* Rules = GetDefault<UWB_ToolsDeveloperSettings>()->SortLevelRules.LoadSynchronous();
	if (!Rules)
	{
		UE_LOG(LogWBSortLevel, Warning, TEXT("Sort Level rules are not set (Project Settings > Editor > WB Tools > Sort Level Rules): nothing is sorted."));
	}
	return Rules;
}

FName UWB_SortLevelLibrary::FindFolderForClass(const UWB_SortLevelRules* Rules, const UClass* ActorClass)
{
	if (!Rules || !ActorClass)
	{
		return NAME_None;
	}
	const FWB_SortClassRule* Rule = WBSortLevel::FindRule(Rules, ActorClass);
	return Rule ? Rule->Folder : NAME_None;
}

FWB_SortLevelReport UWB_SortLevelLibrary::PreviewSortLevel()
{
	return Run(false);
}

FWB_SortLevelReport UWB_SortLevelLibrary::ApplySortLevel()
{
	return Run(true);
}

FWB_SortLevelReport UWB_SortLevelLibrary::Run(bool bApply)
{
	using namespace WBSortLevel;

	FWB_SortLevelReport Report;
	if (!GEditor || GEditor->PlayWorld)
	{
		Report.Warnings.Add(TEXT("Stop Play In Editor before sorting the level."));
		return Report;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UWB_SortLevelRules* Rules = LoadRules();
	if (!World || !Rules)
	{
		Report.Warnings.Add(!World ? TEXT("No map is open in the editor.") : TEXT("Sort Level rules are not set in Project Settings > Editor > WB Tools."));
		return Report;
	}

	// ---------------------------------------------------------------- Map-wide checks
	const bool bPartitioned = World->IsPartitionedWorld();
	if (bPartitioned)
	{
		Report.Warnings.Add(TEXT("World Partition map: only the Outliner folders are sorted, actors keep their level."));
	}
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (Streaming && !Streaming->GetLoadedLevel())
		{
			Report.Warnings.Add(FString::Printf(TEXT("Sub-level %s is not loaded: its actors are not sorted. Load it and sort again."),
				*FPackageName::GetShortName(Streaming->GetWorldAssetPackageName())));
		}
	}

	TMap<FName, ULevel*> LevelOfRoot;
	if (!bPartitioned)
	{
		for (const FWB_SortLevelRule& LevelRule : Rules->LevelRules)
		{
			ULevel* Level = FindSubLevel(World, LevelRule.SubLevelSuffix, Report.Warnings);
			if (!Level)
			{
				Report.Warnings.Add(FString::Printf(TEXT("No loaded sub-level ends with '%s': the actors of %s keep their level."),
					*LevelRule.SubLevelSuffix, *LevelRule.RootFolder.ToString()));
			}
			LevelOfRoot.Add(LevelRule.RootFolder, Level);
		}
	}

	// ---------------------------------------------------------------- Plan
	TArray<FPlan> Plans;
	TMap<AActor*, ULevel*> DestOf;
	bool bHasLevelSequence = false;

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}
		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}
			bHasLevelSequence |= Actor->GetClass()->GetName().Contains(TEXT("LevelSequenceActor"));

			if (const AGroupActor* Group = Cast<AGroupActor>(Actor))
			{
				Report.SkippedGroups.Add(FString::Printf(TEXT("Group %s (%d actors, %s)"), *Group->GetActorLabel(), Group->GroupActors.Num(), *LevelDisplayName(Level)));
				continue;
			}
			if (!IsSortable(Actor) || IsIgnored(Rules, Actor->GetClass()) || Actor->GroupActor)
			{
				continue;
			}
			if (const AActor* Parent = Actor->GetAttachParentActor())
			{
				Report.SkippedGroups.Add(FString::Printf(TEXT("%s (attached to %s)"), *Actor->GetActorLabel(), *Parent->GetActorLabel()));
				continue;
			}

			const FName CurrentFolder = Actor->GetFolderPath();
			FPlan Plan;
			Plan.Actor = Actor;
			Plan.DestLevel = Level;

			if (const FWB_SortClassRule* Rule = FindRule(Rules, Actor->GetClass()))
			{
				Plan.Folder = IsInFolder(CurrentFolder, Rule->Folder) ? CurrentFolder : Rule->Folder;
				if (Rule->bStayInPersistentLevel)
				{
					Plan.DestLevel = World->PersistentLevel;
				}
				else if (!bPartitioned)
				{
					const FName Root = RootOf(Rule->Folder);
					if (ULevel* const* Found = LevelOfRoot.Find(Root))
					{
						if (*Found)
						{
							Plan.DestLevel = *Found;
						}
						else
						{
							AddWarning(Plan.Move, TEXT("Its sub-level is missing: it keeps its level."));
						}
					}
					else
					{
						AddWarning(Plan.Move, FString::Printf(TEXT("No level rule for root folder %s: it keeps its level."), *Root.ToString()));
					}
				}
			}
			else
			{
				Plan.Move.bUnsorted = true;
				Plan.Folder = IsInFolder(CurrentFolder, Rules->UnsortedFolder) ? CurrentFolder : Rules->UnsortedFolder;
				AddWarning(Plan.Move, TEXT("No rule for this class: put it away by hand, or add a rule."));
			}
			if (bPartitioned)
			{
				Plan.DestLevel = Level;
			}

			DestOf.Add(Actor, Plan.DestLevel);
			if (Plan.Folder == CurrentFolder && Plan.DestLevel == Level && !Plan.Move.bUnsorted)
			{
				++Report.NumAlreadySorted;
				continue;
			}

			Plan.Move.ActorLabel = Actor->GetActorLabel();
			Plan.Move.ActorClass = Actor->GetClass()->GetName();
			Plan.Move.FromFolder = CurrentFolder.IsNone() ? FString() : CurrentFolder.ToString();
			Plan.Move.ToFolder = Plan.Folder.ToString();
			Plan.Move.FromLevel = LevelDisplayName(Level);
			Plan.Move.ToLevel = LevelDisplayName(Plan.DestLevel);
			Plans.Add(MoveTemp(Plan));
		}
	}

	if (bHasLevelSequence)
	{
		Report.Warnings.Add(TEXT("This map has Level Sequences: check their bindings after moving actors to another level."));
	}

	// ---------------------------------------------------------------- References that a level move breaks
	TMap<AActor*, int32> PlanOfActor;
	for (int32 Index = 0; Index < Plans.Num(); ++Index)
	{
		if (Plans[Index].DestLevel != Plans[Index].Actor->GetLevel())
		{
			PlanOfActor.Add(Plans[Index].Actor, Index);
		}
	}
	if (PlanOfActor.Num() > 0)
	{
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level)
			{
				continue;
			}
			for (AActor* Referencer : Level->Actors)
			{
				if (!IsValid(Referencer))
				{
					continue;
				}
				ULevel* const* ReferencerDestPtr = DestOf.Find(Referencer);
				ULevel* ReferencerDest = ReferencerDestPtr ? *ReferencerDestPtr : Referencer->GetLevel();

				TArray<UObject*> References;
				FReferenceFinder Finder(References, nullptr, /*bRequireDirectOuter=*/false, /*bShouldIgnoreArchetype=*/true, /*bSerializeRecursively=*/false, /*bShouldIgnoreTransient=*/true);
				Finder.FindReferences(Referencer);

				for (UObject* Object : References)
				{
					AActor* Target = Cast<AActor>(Object);
					if (!Target)
					{
						const UActorComponent* Component = Cast<UActorComponent>(Object);
						Target = Component ? Component->GetOwner() : nullptr;
					}
					if (!Target || Target == Referencer)
					{
						continue;
					}
					ULevel* const* TargetDestPtr = DestOf.Find(Target);
					ULevel* TargetDest = TargetDestPtr ? *TargetDestPtr : Target->GetLevel();
					if (TargetDest == ReferencerDest)
					{
						continue;
					}
					const FString ReferencerName = Referencer->IsA<ALevelScriptActor>() ? FString::Printf(TEXT("the Level Blueprint of %s"), *LevelDisplayName(Referencer->GetLevel())) : Referencer->GetActorLabel();
					if (const int32* TargetPlan = PlanOfActor.Find(Target))
					{
						AddWarning(Plans[*TargetPlan].Move, FString::Printf(TEXT("Referenced by %s: this reference will break."), *ReferencerName));
					}
					else if (const int32* ReferencerPlan = PlanOfActor.Find(Referencer))
					{
						AddWarning(Plans[*ReferencerPlan].Move, FString::Printf(TEXT("References %s, which stays in another level: this reference will break."), *Target->GetActorLabel()));
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------- Apply
	if (bApply && Plans.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("SortLevel", "Sort Level"));

		TMap<ULevel*, TArray<AActor*>> ActorsByDest;
		for (const FPlan& Plan : Plans)
		{
			Plan.Actor->Modify();
			Plan.Actor->SetFolderPath(Plan.Folder);
			if (Plan.DestLevel != Plan.Actor->GetLevel())
			{
				ActorsByDest.FindOrAdd(Plan.DestLevel).Add(Plan.Actor);
			}
		}

		for (const TPair<ULevel*, TArray<AActor*>>& Pair : ActorsByDest)
		{
			TArray<AActor*> MovedActors;
			const int32 NumMoved = UEditorLevelUtils::MoveActorsToLevel(Pair.Value, Pair.Key, /*bWarnAboutReferences=*/false, /*bWarnAboutRenaming=*/false, /*bMoveAllOrFail=*/false, &MovedActors);
			if (NumMoved != Pair.Value.Num())
			{
				Report.Warnings.Add(FString::Printf(TEXT("Only %d of %d actors could be moved to %s (is the sub-level locked or hidden?)."), NumMoved, Pair.Value.Num(), *LevelDisplayName(Pair.Key)));
			}
			// The move copies the actors: make sure the copies keep their sorted folder.
			for (AActor* Moved : MovedActors)
			{
				const FWB_SortClassRule* Rule = IsValid(Moved) ? FindRule(Rules, Moved->GetClass()) : nullptr;
				if (Rule && !IsInFolder(Moved->GetFolderPath(), Rule->Folder))
				{
					Moved->SetFolderPath(Rule->Folder);
				}
			}
		}
		Report.bApplied = true;
	}

	for (const FPlan& Plan : Plans)
	{
		Report.Moves.Add(Plan.Move);
	}
	UE_LOG(LogWBSortLevel, Log, TEXT("%s"), *ReportToText(Report));
	return Report;
}

FString UWB_SortLevelLibrary::ReportToText(const FWB_SortLevelReport& Report)
{
	TArray<FString> Lines;
	int32 NumUnsorted = 0;
	for (const FWB_SortLevelMove& Move : Report.Moves)
	{
		NumUnsorted += Move.bUnsorted ? 1 : 0;
	}
	Lines.Add(FString::Printf(TEXT("Sort Level %s: %d actor(s) to move, %d unsorted, %d already in place, %d group(s) or attached actor(s) skipped."),
		Report.bApplied ? TEXT("applied") : TEXT("preview"), Report.Moves.Num() - NumUnsorted, NumUnsorted, Report.NumAlreadySorted, Report.SkippedGroups.Num()));
	for (const FString& Warning : Report.Warnings)
	{
		Lines.Add(TEXT("  WARNING: ") + Warning);
	}
	for (const FWB_SortLevelMove& Move : Report.Moves)
	{
		Lines.Add(FString::Printf(TEXT("  %s%s [%s]: %s | %s -> %s | %s%s"),
			Move.bUnsorted ? TEXT("UNSORTED ") : TEXT(""), *Move.ActorLabel, *Move.ActorClass,
			*Move.FromFolder, *Move.FromLevel, *Move.ToFolder, *Move.ToLevel,
			Move.Warning.IsEmpty() ? TEXT("") : *(TEXT(" WARNING: ") + Move.Warning)));
	}
	if (Report.SkippedGroups.Num() > 0)
	{
		Lines.Add(TEXT("  Skipped (groups and attached actors are never moved):"));
		for (const FString& Group : Report.SkippedGroups)
		{
			Lines.Add(TEXT("    ") + Group);
		}
	}
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
