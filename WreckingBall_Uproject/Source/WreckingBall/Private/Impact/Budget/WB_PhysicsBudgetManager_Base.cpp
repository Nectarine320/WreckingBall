#include "Impact/Budget/WB_PhysicsBudgetManager_Base.h"

#include "Impact/Budget/WB_PhysicsBudgetData_Base.h"

#include "Algo/Sort.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionProxyData.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBDebris, Log, All);

namespace WBDebris
{
	// Rate of the shrink animation. The engine applies the decay to the render at the next physics sync.
	constexpr float ShrinkUpdateInterval = 1.f / 30.f;
	constexpr float MinInterval = 0.05f;

	static void LogStatsForWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AWB_PhysicsBudgetManager_Base> It(World); It; ++It)
		{
			UE_LOG(LogWBDebris, Display, TEXT("%s: %s"), *It->GetName(), *It->BuildStatsString());
			++Count;
		}

		if (Count == 0)
		{
			UE_LOG(LogWBDebris, Display, TEXT("No physics budget manager in this world."));
		}
	}

	static FAutoConsoleCommandWithWorld StatsCommand(
		TEXT("wb.Debris.Stats"),
		TEXT("Logs the counters of the physics budget managers of the current world."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&LogStatsForWorld));

	static const TCHAR* ModeToString(EWB_DebrisRemovalMode Mode)
	{
		switch (Mode)
		{
		case EWB_DebrisRemovalMode::Budget:    return TEXT("Budget");
		case EWB_DebrisRemovalMode::Emergency: return TEXT("Emergency");
		default:                               return TEXT("Wear");
		}
	}
}

AWB_PhysicsBudgetManager_Base::AWB_PhysicsBudgetManager_Base()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWB_PhysicsBudgetManager_Base::BeginPlay()
{
	Super::BeginPlay();

	if (!BudgetData)
	{
		UE_LOG(LogWBDebris, Warning, TEXT("%s: BudgetData is not set. Per-piece debris removal is off and every registration will be ignored."), *GetName());
	}
	else if (!BudgetData->bUsePerPieceDebris)
	{
		UE_LOG(LogWBDebris, Log, TEXT("%s: bUsePerPieceDebris is false in %s. Per-piece debris removal is off."), *GetName(), *BudgetData->GetName());
	}

	if (IsPerPieceEnabled())
	{
		ScheduleNextPass(BudgetData->WearInterval);
	}
}

void AWB_PhysicsBudgetManager_Base::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PassTimerHandle);
	GetWorldTimerManager().ClearTimer(ShrinkTimerHandle);
	Collections.Reset();
	ShrinkingPieces.Reset();
	PhysicsProps.Reset();

	Super::EndPlay(EndPlayReason);
}

bool AWB_PhysicsBudgetManager_Base::IsPerPieceEnabled() const
{
	return BudgetData && BudgetData->bUsePerPieceDebris;
}

// ---------------------------------------------------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------------------------------------------------

void AWB_PhysicsBudgetManager_Base::RegisterGeometryCollection(UGeometryCollectionComponent* Collection)
{
	if (!IsValid(Collection) || !IsPerPieceEnabled() || FindCollectionIndex(Collection) != INDEX_NONE)
	{
		return;
	}

	const UGeometryCollection* RestCollection = Collection->GetRestCollection();
	FGeometryDynamicCollection* DynamicCollection = Collection->GetDynamicCollection();
	if (!RestCollection || !RestCollection->GetGeometryCollection().IsValid() || !DynamicCollection)
	{
		UE_LOG(LogWBDebris, Warning, TEXT("RegisterGeometryCollection: %s has no rest or dynamic collection, ignored."), *Collection->GetPathName());
		return;
	}

	const FGeometryCollection& GeometryCollection = *RestCollection->GetGeometryCollection();
	const int32 NumTransforms = GeometryCollection.NumElements(FGeometryCollection::TransformGroup);
	if (NumTransforms != DynamicCollection->GetNumTransforms())
	{
		UE_LOG(LogWBDebris, Warning, TEXT("RegisterGeometryCollection: %s rest and dynamic collections do not match, ignored."), *Collection->GetPathName());
		return;
	}

	FWB_TrackedCollection& Tracked = Collections.AddDefaulted_GetRef();
	Tracked.Component = Collection;
	Tracked.RegisterTime = GetWorld()->GetTimeSeconds();
	Tracked.RootIndex = Collection->GetRootIndex();
	Tracked.Pieces.SetNum(NumTransforms);

	// Piece volume = bounding box volume of its geometry, scaled by the component.
	// A cluster gets the sum of the volumes of all the geometry under it.
	const FVector Scale = Collection->GetComponentScale();
	const double ScaleFactor = FMath::Abs(Scale.X * Scale.Y * Scale.Z);
	const TManagedArray<int32>& TransformToGeometry = GeometryCollection.TransformToGeometryIndex;
	const TManagedArray<FBox>& BoundingBoxes = GeometryCollection.BoundingBox;
	const TManagedArray<int32>& Parents = GeometryCollection.Parent;

	for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
	{
		const int32 GeometryIndex = TransformToGeometry[TransformIndex];
		if (GeometryIndex < 0 || GeometryIndex >= BoundingBoxes.Num())
		{
			continue;
		}

		const FVector Size = BoundingBoxes[GeometryIndex].GetSize();
		const float Volume = static_cast<float>(Size.X * Size.Y * Size.Z * ScaleFactor);
		for (int32 Index = TransformIndex; Index != INDEX_NONE; Index = Parents[Index])
		{
			Tracked.Pieces[Index].Volume += Volume;
		}
	}

	// Calibration help for SmallPieceVolume / ProtectedPieceVolume: volume spread of the leaf pieces (cm3).
	{
		TArray<float> LeafVolumes;
		LeafVolumes.Reserve(NumTransforms);
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			if (TransformIndex != Tracked.RootIndex && TransformToGeometry[TransformIndex] != INDEX_NONE)
			{
				LeafVolumes.Add(Tracked.Pieces[TransformIndex].Volume);
			}
		}
		if (LeafVolumes.Num() > 0)
		{
			LeafVolumes.Sort();
			const int32 Last = LeafVolumes.Num() - 1;
			UE_LOG(LogWBDebris, Log, TEXT("Registered %s (%s): %d pieces, volume cm3 min %.0f | p25 %.0f | median %.0f | p75 %.0f | p90 %.0f | max %.0f"),
				*Collection->GetName(), *RestCollection->GetName(), LeafVolumes.Num(),
				LeafVolumes[0], LeafVolumes[Last / 4], LeafVolumes[Last / 2], LeafVolumes[(Last * 3) / 4], LeafVolumes[(Last * 9) / 10], LeafVolumes[Last]);
		}
	}

	// The engine applies the decay attribute (shrink, then zero scale) only when removal is allowed on the instance.
	// The engine's own Remove On Sleep timer stays off as long as the asset does not enable it.
	Collection->bAllowRemovalOnSleep = true;
	FGeometryCollectionDecayDynamicFacade DecayFacade(*DynamicCollection);
	DecayFacade.AddAttributes();

	if (RestCollection->bRemoveOnMaxSleep)
	{
		UE_LOG(LogWBDebris, Warning, TEXT("%s: 'Remove on Sleep' is enabled on the asset. The engine will also remove pieces. Disable it on the asset."), *RestCollection->GetName());
	}
	if (!RestCollection->bScaleOnRemoval && !bLoggedNoScaleOnRemoval)
	{
		bLoggedNoScaleOnRemoval = true;
		UE_LOG(LogWBDebris, Warning, TEXT("%s: 'Scale On Removal' is disabled on the asset. Removed pieces will vanish at the end of the shrink instead of shrinking."), *RestCollection->GetName());
	}

	if (!GetWorldTimerManager().IsTimerActive(PassTimerHandle))
	{
		ScheduleNextPass(BudgetData->ThinInterval);
	}
}

void AWB_PhysicsBudgetManager_Base::UnregisterGeometryCollection(UGeometryCollectionComponent* Collection)
{
	const int32 CollectionIndex = FindCollectionIndex(Collection);
	if (CollectionIndex == INDEX_NONE)
	{
		return;
	}

	ShrinkingPieces.RemoveAllSwap([Collection](const FWB_ShrinkingPiece& Piece)
	{
		return Piece.Component.Get() == Collection;
	});
	Collections.RemoveAtSwap(CollectionIndex);
}

void AWB_PhysicsBudgetManager_Base::RegisterPhysicsProp(UPrimitiveComponent* Prop)
{
	if (IsValid(Prop))
	{
		PhysicsProps.AddUnique(Prop);
	}
}

void AWB_PhysicsBudgetManager_Base::UnregisterPhysicsProp(UPrimitiveComponent* Prop)
{
	PhysicsProps.RemoveSwap(Prop);
}

int32 AWB_PhysicsBudgetManager_Base::FindCollectionIndex(const UGeometryCollectionComponent* Collection) const
{
	return Collections.IndexOfByPredicate([Collection](const FWB_TrackedCollection& Tracked)
	{
		return Tracked.Component.Get() == Collection;
	});
}

void AWB_PhysicsBudgetManager_Base::RemoveInvalidCollections()
{
	for (int32 Index = Collections.Num() - 1; Index >= 0; --Index)
	{
		if (!Collections[Index].Component.IsValid())
		{
			Collections.RemoveAtSwap(Index);
		}
	}

	PhysicsProps.RemoveAllSwap([](const TWeakObjectPtr<UPrimitiveComponent>& Prop)
	{
		return !Prop.IsValid();
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Removal passes
// ---------------------------------------------------------------------------------------------------------------------

void AWB_PhysicsBudgetManager_Base::ScheduleNextPass(float Interval)
{
	GetWorldTimerManager().SetTimer(PassTimerHandle, this, &AWB_PhysicsBudgetManager_Base::RunPass, FMath::Max(Interval, WBDebris::MinInterval), false);
}

void AWB_PhysicsBudgetManager_Base::RunPass()
{
	if (!IsPerPieceEnabled())
	{
		return; // Restarted by the next RegisterGeometryCollection.
	}

	const UWB_PhysicsBudgetData_Base& Data = *BudgetData;
	const double Now = GetWorld()->GetTimeSeconds();

	RemoveInvalidCollections();
	GatherPlayerLocations();

	// 1. Update the state of every tracked piece.
	int32 ActivePieces = 0;
	int32 SleepingPieces = 0;
	int32 AwakePieces = 0;

	for (int32 CollectionIndex = Collections.Num() - 1; CollectionIndex >= 0; --CollectionIndex)
	{
		FWB_TrackedCollection& Tracked = Collections[CollectionIndex];
		Tracked.RemovalsThisPass = 0;

		UGeometryCollectionComponent* Component = Tracked.Component.Get();
		FGeometryDynamicCollection* DynamicCollection = Component->GetDynamicCollection();
		if (!DynamicCollection)
		{
			continue;
		}

		FGeometryCollectionDynamicStateFacade StateFacade(*DynamicCollection);
		if (!StateFacade.IsValid())
		{
			continue;
		}

		bool bHasActiveTransform = false;
		bool bHasShrinkingPiece = false;
		const int32 NumTransforms = FMath::Min(Tracked.Pieces.Num(), DynamicCollection->GetNumTransforms());
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			FWB_TrackedPiece& Piece = Tracked.Pieces[TransformIndex];
			bHasShrinkingPiece |= Piece.bShrinking;
			bHasActiveTransform |= (!Piece.bRemoved && StateFacade.IsActive(TransformIndex));

			if (TransformIndex == Tracked.RootIndex || Piece.bRemoved || !StateFacade.HasBrokenOff(TransformIndex))
			{
				Piece.SleepStartTime = -1.0;
				continue;
			}

			++ActivePieces;
			if (StateFacade.IsSleeping(TransformIndex))
			{
				++SleepingPieces;
				if (Piece.SleepStartTime < 0.0)
				{
					Piece.SleepStartTime = Now;
				}
			}
			else
			{
				AwakePieces += Piece.bShrinking ? 0 : 1;
				Piece.SleepStartTime = -1.0;
			}
		}

		// Every piece has been removed: the component itself can go.
		if (!bHasActiveTransform && !bHasShrinkingPiece)
		{
			Collections.RemoveAtSwap(CollectionIndex);
			Component->DestroyComponent();
		}
	}

	// 2. Choose the mode. Emergency ends as soon as the count is back under the budget (no back-and-forth).
	const int32 MaxPieces = Data.MaxDebrisPieces;
	const int32 CountedPieces = FMath::Max(ActivePieces - ShrinkingPieces.Num(), 0);

	if (bInEmergency && CountedPieces <= MaxPieces)
	{
		bInEmergency = false;
		bLoggedAwakeEmergency = false;
	}
	if (!bInEmergency && CountedPieces > MaxPieces * Data.EmergencyThresholdRatio)
	{
		bInEmergency = true;
	}

	const EWB_DebrisRemovalMode Mode = bInEmergency ? EWB_DebrisRemovalMode::Emergency
		: (CountedPieces > MaxPieces ? EWB_DebrisRemovalMode::Budget : EWB_DebrisRemovalMode::Wear);

	// 3-5. Select, sort and remove.
	float NextInterval = Data.WearInterval;
	if (Mode == EWB_DebrisRemovalMode::Wear)
	{
		SelectCandidates(false, true, Now);
		RemoveCandidates(Data.MaxWearRemovalsPerPass, Data.MaxRemovalsPerCollectionPerPass, Data.ShrinkDuration);
	}
	else
	{
		const bool bEmergency = (Mode == EWB_DebrisRemovalMode::Emergency);
		const int32 PassLimit = bEmergency ? Data.EmergencyRemovalsPerPass : Data.MaxRemovalsPerPass;
		const int32 Limit = FMath::Min(CountedPieces - MaxPieces, PassLimit);
		const int32 PerCollection = bEmergency ? Data.EmergencyRemovalsPerCollectionPerPass : Data.MaxRemovalsPerCollectionPerPass;
		const float Duration = bEmergency ? Data.EmergencyShrinkDuration : Data.ShrinkDuration;
		NextInterval = bEmergency ? Data.EmergencyInterval : Data.ThinInterval;

		SelectCandidates(false, false, Now);
		if (Candidates.Num() == 0)
		{
			// No regular candidate left: large pieces become candidates.
			SelectCandidates(true, false, Now);
		}

		const int32 Removed = RemoveCandidates(Limit, PerCollection, Duration);

		// 6. Awake pieces are never removed. Report it once per emergency episode.
		if (bEmergency && Removed < Limit && AwakePieces > 0 && !bLoggedAwakeEmergency)
		{
			bLoggedAwakeEmergency = true;
			UE_LOG(LogWBDebris, Warning, TEXT("Emergency: %d active pieces for a budget of %d, but %d pieces are awake and cannot be removed."), CountedPieces, MaxPieces, AwakePieces);
		}
	}

	CachedStats.Mode = Mode;
	CachedStats.ActivePieces = ActivePieces;
	CachedStats.SleepingPieces = SleepingPieces;

	if (Data.bShowDebugStats && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), NextInterval + 0.2f, FColor::Cyan, BuildStatsString());
	}

	ScheduleNextPass(NextInterval);
}

void AWB_PhysicsBudgetManager_Base::GatherPlayerLocations()
{
	PlayerLocations.Reset();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (Pawn)
		{
			PlayerLocations.Add(Pawn->GetActorLocation());
		}
	}

	if (BudgetData->DistanceMode == EWB_DebrisDistanceMode::PlayersAverage && PlayerLocations.Num() > 1)
	{
		FVector Average = FVector::ZeroVector;
		for (const FVector& Location : PlayerLocations)
		{
			Average += Location;
		}
		Average /= PlayerLocations.Num();
		PlayerLocations.Reset();
		PlayerLocations.Add(Average);
	}
}

double AWB_PhysicsBudgetManager_Base::ComputeDistanceSq(const FVector& Location) const
{
	if (PlayerLocations.Num() == 0)
	{
		return 0.0;
	}

	double Best = TNumericLimits<double>::Max();
	for (const FVector& PlayerLocation : PlayerLocations)
	{
		Best = FMath::Min(Best, FVector::DistSquared(Location, PlayerLocation));
	}
	return Best;
}

void AWB_PhysicsBudgetManager_Base::SelectCandidates(bool bIncludeProtected, bool bWearOnly, double Now)
{
	const UWB_PhysicsBudgetData_Base& Data = *BudgetData;
	Candidates.Reset();

	for (int32 CollectionIndex = 0; CollectionIndex < Collections.Num(); ++CollectionIndex)
	{
		FWB_TrackedCollection& Tracked = Collections[CollectionIndex];
		UGeometryCollectionComponent* Component = Tracked.Component.Get();
		FGeometryDynamicCollection* DynamicCollection = Component ? Component->GetDynamicCollection() : nullptr;
		FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
		if (!DynamicCollection || !Proxy)
		{
			continue;
		}

		FGeometryCollectionDynamicStateFacade StateFacade(*DynamicCollection);
		if (!StateFacade.IsValid())
		{
			continue;
		}

		const int32 NumTransforms = FMath::Min(Tracked.Pieces.Num(), DynamicCollection->GetNumTransforms());
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			const FWB_TrackedPiece& Piece = Tracked.Pieces[TransformIndex];
			if (TransformIndex == Tracked.RootIndex || Piece.bShrinking || Piece.bRemoved
				|| !StateFacade.HasBrokenOff(TransformIndex) || !StateFacade.IsSleeping(TransformIndex))
			{
				continue;
			}

			const bool bSmall = Data.SmallPieceVolume > 0.f && Piece.Volume < Data.SmallPieceVolume;
			const bool bProtected = Data.ProtectedPieceVolume > 0.f && Piece.Volume > Data.ProtectedPieceVolume;
			if (bProtected && !bIncludeProtected)
			{
				continue;
			}
			if (bWearOnly && (!bSmall || Piece.SleepStartTime < 0.0 || Now - Piece.SleepStartTime < Data.SmallPieceLifetime))
			{
				continue;
			}

			const FGeometryCollectionPhysicsProxy::FParticle* Particle = Proxy->GetParticleByIndex_External(TransformIndex);
			const FVector Location = Particle ? FVector(Particle->GetX()) : Component->GetComponentLocation();

			FWB_DebrisCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.CollectionIndex = CollectionIndex;
			Candidate.TransformIndex = TransformIndex;
			Candidate.Volume = Piece.Volume;
			Candidate.RegisterTime = Tracked.RegisterTime;
			Candidate.DistanceSq = ComputeDistanceSq(Location);
		}
	}

	// Smallest first, then oldest, then farthest from the players.
	Algo::Sort(Candidates, [](const FWB_DebrisCandidate& A, const FWB_DebrisCandidate& B)
	{
		if (A.Volume != B.Volume)
		{
			return A.Volume < B.Volume;
		}
		if (A.RegisterTime != B.RegisterTime)
		{
			return A.RegisterTime < B.RegisterTime;
		}
		return A.DistanceSq > B.DistanceSq;
	});
}

int32 AWB_PhysicsBudgetManager_Base::RemoveCandidates(int32 MaxRemovals, int32 MaxPerCollection, float Duration)
{
	int32 Removed = 0;
	for (const FWB_DebrisCandidate& Candidate : Candidates)
	{
		if (Removed >= MaxRemovals)
		{
			break;
		}

		FWB_TrackedCollection& Tracked = Collections[Candidate.CollectionIndex];
		if (Tracked.RemovalsThisPass >= MaxPerCollection)
		{
			continue; // This collection had its share; the rest waits for the next pass.
		}

		StartShrink(Candidate.CollectionIndex, Candidate.TransformIndex, Duration);
		++Tracked.RemovalsThisPass;
		++Removed;
	}
	return Removed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shrink, then disable
// ---------------------------------------------------------------------------------------------------------------------

void AWB_PhysicsBudgetManager_Base::StartShrink(int32 CollectionIndex, int32 TransformIndex, float Duration)
{
	FWB_TrackedCollection& Tracked = Collections[CollectionIndex];
	UGeometryCollectionComponent* Component = Tracked.Component.Get();
	FGeometryDynamicCollection* DynamicCollection = Component ? Component->GetDynamicCollection() : nullptr;
	if (!DynamicCollection)
	{
		return;
	}

	FGeometryCollectionDecayDynamicFacade DecayFacade(*DynamicCollection);
	if (!DecayFacade.IsValid())
	{
		DecayFacade.AddAttributes();
	}

	Tracked.Pieces[TransformIndex].bShrinking = true;

	FWB_ShrinkingPiece& Shrinking = ShrinkingPieces.AddDefaulted_GetRef();
	Shrinking.Component = Component;
	Shrinking.TransformIndex = TransformIndex;
	Shrinking.StartTime = GetWorld()->GetTimeSeconds();
	Shrinking.Duration = FMath::Max(Duration, WBDebris::MinInterval);

	if (!GetWorldTimerManager().IsTimerActive(ShrinkTimerHandle))
	{
		GetWorldTimerManager().SetTimer(ShrinkTimerHandle, this, &AWB_PhysicsBudgetManager_Base::UpdateShrinkingPieces, WBDebris::ShrinkUpdateInterval, true);
	}
}

void AWB_PhysicsBudgetManager_Base::UpdateShrinkingPieces()
{
	const double Now = GetWorld()->GetTimeSeconds();
	FinishedPieces.Reset();

	for (int32 Index = ShrinkingPieces.Num() - 1; Index >= 0; --Index)
	{
		const FWB_ShrinkingPiece& Shrinking = ShrinkingPieces[Index];
		UGeometryCollectionComponent* Component = Shrinking.Component.Get();
		FGeometryDynamicCollection* DynamicCollection = Component ? Component->GetDynamicCollection() : nullptr;
		if (!DynamicCollection)
		{
			ShrinkingPieces.RemoveAtSwap(Index);
			continue;
		}

		// Decay 0..1 drives the engine's removal: the piece scales down, then reaches zero scale.
		const float Alpha = FMath::Clamp(static_cast<float>((Now - Shrinking.StartTime) / Shrinking.Duration), 0.f, 1.f);
		FGeometryCollectionDecayDynamicFacade DecayFacade(*DynamicCollection);
		if (DecayFacade.IsValid() && Shrinking.TransformIndex < DecayFacade.GetDecayAttributeSize())
		{
			DecayFacade.SetDecay(Shrinking.TransformIndex, FMath::Max(Alpha, UE_KINDA_SMALL_NUMBER));
		}

		if (Alpha >= 1.f)
		{
			FinishedPieces.Add(Shrinking);
			ShrinkingPieces.RemoveAtSwap(Index);
		}
	}

	if (ShrinkingPieces.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(ShrinkTimerHandle);
	}

	// Done after the loop: OnPieceRemoved may call back into the manager.
	for (const FWB_ShrinkingPiece& Finished : FinishedPieces)
	{
		FinishShrink(Finished);
	}
}

void AWB_PhysicsBudgetManager_Base::FinishShrink(const FWB_ShrinkingPiece& Piece)
{
	UGeometryCollectionComponent* Component = Piece.Component.Get();
	if (!Component)
	{
		return;
	}

	// Takes the piece out of the simulation: no collision, no rendering.
	if (FGeometryCollectionPhysicsProxy* Proxy = Component->GetPhysicsProxy())
	{
		if (Proxy->GetParticleByIndex_External(Piece.TransformIndex))
		{
			TArray<int32> Indices;
			Indices.Add(Piece.TransformIndex);
			Proxy->DisableParticles_External(MoveTemp(Indices));
		}
	}

	const int32 CollectionIndex = FindCollectionIndex(Component);
	if (CollectionIndex != INDEX_NONE)
	{
		FWB_TrackedPiece& Tracked = Collections[CollectionIndex].Pieces[Piece.TransformIndex];
		Tracked.bShrinking = false;
		Tracked.bRemoved = true;
	}

	++CachedStats.TotalRemovedPieces;
	OnPieceRemoved.Broadcast(Component, Piece.TransformIndex);
}

// ---------------------------------------------------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------------------------------------------------

FWB_PhysicsBudgetStats AWB_PhysicsBudgetManager_Base::GetDebrisStats() const
{
	FWB_PhysicsBudgetStats Stats = CachedStats;
	Stats.ShrinkingPieces = ShrinkingPieces.Num();
	Stats.RegisteredCollections = Collections.Num();
	Stats.MaxPieces = BudgetData ? BudgetData->MaxDebrisPieces : 0;
	Stats.PhysicsPropsReserve = BudgetData ? BudgetData->PhysicsPropsReserve : 0;

	Stats.RegisteredProps = 0;
	Stats.SimulatedProps = 0;
	for (const TWeakObjectPtr<UPrimitiveComponent>& Prop : PhysicsProps)
	{
		if (const UPrimitiveComponent* Component = Prop.Get())
		{
			++Stats.RegisteredProps;
			Stats.SimulatedProps += Component->IsSimulatingPhysics() ? 1 : 0;
		}
	}
	return Stats;
}

FString AWB_PhysicsBudgetManager_Base::BuildStatsString() const
{
	const FWB_PhysicsBudgetStats Stats = GetDebrisStats();
	return FString::Printf(TEXT("Debris [%s] active %d/%d | sleeping %d | shrinking %d | removed %d | GC %d | props simulated %d/%d (reserve %d)"),
		WBDebris::ModeToString(Stats.Mode), Stats.ActivePieces, Stats.MaxPieces, Stats.SleepingPieces, Stats.ShrinkingPieces,
		Stats.TotalRemovedPieces, Stats.RegisteredCollections, Stats.SimulatedProps, Stats.RegisteredProps, Stats.PhysicsPropsReserve);
}
