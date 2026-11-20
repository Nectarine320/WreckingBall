#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Impact/Budget/WB_PhysicsBudgetTypes.h"
#include "WB_PhysicsBudgetData_Base.generated.h"

/**
 * Settings of the per-piece debris manager and of the physics budget.
 * Base class of PDA_PhysicsBudget.
 */
UCLASS(Blueprintable, BlueprintType)
class WRECKINGBALL_API UWB_PhysicsBudgetData_Base : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Enables the per-piece debris manager. When false, nothing is removed except by the wall's fallback timer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris")
	bool bUsePerPieceDebris = false;

	/** Maximum number of active Geometry Collection pieces on the map. Measure it in the stress test (start with the pieces of 6 collapsed walls). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Budget", meta = (ClampMin = "0"))
	int32 MaxDebrisPieces = 200;

	/** Delay in seconds between two removal passes in Budget mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Thinning", meta = (ClampMin = "0.05", Units = "s"))
	float ThinInterval = 0.5f;

	/** Maximum number of pieces removed per pass in Budget mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Thinning", meta = (ClampMin = "1"))
	int32 MaxRemovalsPerPass = 4;

	/** Maximum number of pieces removed from the same Geometry Collection per pass (Budget and Wear modes). Spreads the removal across the map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Thinning", meta = (ClampMin = "1"))
	int32 MaxRemovalsPerCollectionPerPass = 1;

	/** Pieces below this volume (cm3) are small. Small pieces are removed first and are the only ones affected by wear. 0 = no piece is small. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Size", meta = (ClampMin = "0"))
	float SmallPieceVolume = 0.f;

	/** Pieces above this volume (cm3) are large and protected: they are removed only when over budget and no other candidate is left. 0 = no protection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Size", meta = (ClampMin = "0"))
	float ProtectedPieceVolume = 0.f;

	/** A small piece asleep for longer than this (seconds) becomes a wear candidate, even under the budget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Wear", meta = (ClampMin = "0", Units = "s"))
	float SmallPieceLifetime = 20.f;

	/** Delay in seconds between two removal passes in Wear mode. Should be slower than ThinInterval. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Wear", meta = (ClampMin = "0.05", Units = "s"))
	float WearInterval = 2.f;

	/** Maximum number of pieces removed per pass in Wear mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Wear", meta = (ClampMin = "0"))
	int32 MaxWearRemovalsPerPass = 1;

	/** Emergency mode starts above MaxDebrisPieces multiplied by this ratio, and stops as soon as the count is back under MaxDebrisPieces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Emergency", meta = (ClampMin = "1"))
	float EmergencyThresholdRatio = 1.5f;

	/** Delay in seconds between two removal passes in Emergency mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Emergency", meta = (ClampMin = "0.05", Units = "s"))
	float EmergencyInterval = 0.1f;

	/** Maximum number of pieces removed per pass in Emergency mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Emergency", meta = (ClampMin = "1"))
	int32 EmergencyRemovalsPerPass = 32;

	/** Maximum number of pieces removed from the same Geometry Collection per pass in Emergency mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Emergency", meta = (ClampMin = "1"))
	int32 EmergencyRemovalsPerCollectionPerPass = 4;

	/** Shrink duration in seconds of a piece removed in Emergency mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Emergency", meta = (ClampMin = "0.05", Units = "s"))
	float EmergencyShrinkDuration = 0.3f;

	/**
	 * Over the budget, when no sleeping piece is left to remove, awake (still moving) pieces can be shrunk too.
	 * Same order as sleeping pieces: smallest first, large protected pieces last, spread across the collections.
	 * Off by default: an awake piece is never removed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Awake")
	bool bAllowAwakeRemoval = false;

	/** An awake piece faster than this speed (cm/s) is never removed, so a piece in flight does not shrink in front of the players. 0 = no speed limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Awake", meta = (ClampMin = "0", Units = "cm/s", EditCondition = "bAllowAwakeRemoval"))
	float AwakeRemovalMaxSpeed = 100.f;

	/** An awake piece closer than this distance (cm) to the players is never removed. Measured with DistanceMode. 0 = no distance limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Awake", meta = (ClampMin = "0", Units = "cm", EditCondition = "bAllowAwakeRemoval"))
	float AwakeRemovalMinPlayerDistance = 500.f;

	/** An awake piece broken off for less than this time (seconds) is never removed, so a wall that just exploded does not shrink at once. 0 = no age limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Awake", meta = (ClampMin = "0", Units = "s", EditCondition = "bAllowAwakeRemoval"))
	float AwakeRemovalMinAge = 3.f;

	/**
	 * When a Geometry Collection breaks, its smallest pieces (SmallestOnBreakShare of them) are shrunk as soon as they break off,
	 * moving or not, whatever the budget. Thins the rubble at once. Off by default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|On Break")
	bool bRemoveSmallestOnBreak = false;

	/** Share (0..1) of the pieces of each collection, the smallest ones by volume, removed as soon as they break off. 0.25 = the smallest quarter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|On Break", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bRemoveSmallestOnBreak"))
	float SmallestOnBreakShare = 0.25f;

	/** Seconds between the moment a marked piece breaks off and the start of its shrink, so the players see it fly first. 0 = at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|On Break", meta = (ClampMin = "0", Units = "s", EditCondition = "bRemoveSmallestOnBreak"))
	float SmallestOnBreakDelay = 0.3f;

	/** Shrink duration in seconds of a piece removed on break. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|On Break", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bRemoveSmallestOnBreak"))
	float SmallestOnBreakShrinkDuration = 1.f;

	/** How the distance to the players is measured when sorting candidates (farthest pieces are removed first). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Distance")
	EWB_DebrisDistanceMode DistanceMode = EWB_DebrisDistanceMode::NearestPlayer;

	/** Shrink duration in seconds of a removed piece before it is disabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debris|Removal", meta = (ClampMin = "0.05", Units = "s"))
	float ShrinkDuration = 1.f;

	/** Number of simulated bodies reserved for the physics props. Measured and displayed only: the manager never removes a prop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Budget|Props", meta = (ClampMin = "0"))
	int32 PhysicsPropsReserve = 0;

	/** Shows the budget counters on screen at every pass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	bool bShowDebugStats = false;
};
