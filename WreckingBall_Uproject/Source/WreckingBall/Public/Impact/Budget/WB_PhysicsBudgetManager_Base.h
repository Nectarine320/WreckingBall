#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Impact/Budget/WB_PhysicsBudgetTypes.h"
#include "WB_PhysicsBudgetManager_Base.generated.h"

class UGeometryCollectionComponent;
class UPrimitiveComponent;
class UWB_PhysicsBudgetData_Base;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnDebrisPieceRemoved, UGeometryCollectionComponent*, Collection, int32, PieceIndex);

/**
 * Removes Geometry Collection debris piece by piece to keep the physics budget.
 * Small, old and far sleeping pieces go first; a removed piece shrinks, then is disabled.
 * Base class of BP_PhysicsBudgetManager.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_PhysicsBudgetManager_Base : public AActor
{
	GENERATED_BODY()

public:
	AWB_PhysicsBudgetManager_Base();

	/** Budget settings. Must be set (DA_PhysicsBudget) before BeginPlay for the per-piece removal to run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
	TObjectPtr<UWB_PhysicsBudgetData_Base> BudgetData;

	/** Called when a piece has finished shrinking and has been disabled. Meant for removal VFX. */
	UPROPERTY(BlueprintAssignable, Category = "Debris|Events")
	FWB_OnDebrisPieceRemoved OnPieceRemoved;

	/** Registers a collapsed Geometry Collection so its broken pieces are managed by the budget. Call it after the collapse. Ignored when bUsePerPieceDebris is false. */
	UFUNCTION(BlueprintCallable, Category = "Debris")
	void RegisterGeometryCollection(UGeometryCollectionComponent* Collection);

	/** Stops managing a Geometry Collection. Destroyed collections are also cleaned up automatically. */
	UFUNCTION(BlueprintCallable, Category = "Debris")
	void UnregisterGeometryCollection(UGeometryCollectionComponent* Collection);

	/** Registers a physics prop for the budget counters. Props are counted only, never removed. */
	UFUNCTION(BlueprintCallable, Category = "Budget|Props")
	void RegisterPhysicsProp(UPrimitiveComponent* Prop);

	/** Unregisters a physics prop from the budget counters. */
	UFUNCTION(BlueprintCallable, Category = "Budget|Props")
	void UnregisterPhysicsProp(UPrimitiveComponent* Prop);

	/** Returns the budget counters. Piece counters are refreshed at every removal pass; prop counters are live. */
	UFUNCTION(BlueprintPure, Category = "Debris|Stats")
	FWB_PhysicsBudgetStats GetDebrisStats() const;

	/** Builds the one-line stats text used by the on-screen debug and the wb.Debris.Stats console command. */
	FString BuildStatsString() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FWB_TrackedPiece
	{
		float Volume = 0.f;
		double SleepStartTime = -1.0;
		bool bShrinking = false;
		bool bRemoved = false;
	};

	struct FWB_TrackedCollection
	{
		TWeakObjectPtr<UGeometryCollectionComponent> Component;
		double RegisterTime = 0.0;
		int32 RootIndex = INDEX_NONE;
		int32 RemovalsThisPass = 0;
		TArray<FWB_TrackedPiece> Pieces;
	};

	struct FWB_DebrisCandidate
	{
		int32 CollectionIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		float Volume = 0.f;
		double RegisterTime = 0.0;
		double DistanceSq = 0.0;
	};

	struct FWB_ShrinkingPiece
	{
		TWeakObjectPtr<UGeometryCollectionComponent> Component;
		int32 TransformIndex = INDEX_NONE;
		double StartTime = 0.0;
		float Duration = 1.f;
	};

	bool IsPerPieceEnabled() const;
	void ScheduleNextPass(float Interval);
	void RunPass();
	void UpdateShrinkingPieces();
	void StartShrink(int32 CollectionIndex, int32 TransformIndex, float Duration);
	void FinishShrink(const FWB_ShrinkingPiece& Piece);
	void RemoveInvalidCollections();
	void GatherPlayerLocations();
	double ComputeDistanceSq(const FVector& Location) const;
	int32 FindCollectionIndex(const UGeometryCollectionComponent* Collection) const;
	void SelectCandidates(bool bIncludeProtected, bool bWearOnly, double Now);
	int32 RemoveCandidates(int32 MaxRemovals, int32 MaxPerCollection, float Duration);

	TArray<FWB_TrackedCollection> Collections;
	TArray<FWB_ShrinkingPiece> ShrinkingPieces;
	TArray<FWB_ShrinkingPiece> FinishedPieces;
	TArray<FWB_DebrisCandidate> Candidates;
	TArray<FVector> PlayerLocations;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> PhysicsProps;

	FTimerHandle PassTimerHandle;
	FTimerHandle ShrinkTimerHandle;

	FWB_PhysicsBudgetStats CachedStats;
	bool bInEmergency = false;
	bool bLoggedAwakeEmergency = false;
	bool bLoggedNoScaleOnRemoval = false;
};
