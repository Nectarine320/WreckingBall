#pragma once

#include "CoreMinimal.h"
#include "WB_PhysicsBudgetTypes.generated.h"

/** How the distance between a debris piece and the players is measured when sorting removal candidates. */
UENUM(BlueprintType)
enum class EWB_DebrisDistanceMode : uint8
{
	/** Distance to the closest player pawn. A piece near any single player is kept longer. */
	NearestPlayer UMETA(DisplayName = "Nearest Player"),

	/** Distance to the average position of all player pawns. */
	PlayersAverage UMETA(DisplayName = "Players Average")
};

/** Current removal mode of the physics budget manager. */
UENUM(BlueprintType)
enum class EWB_DebrisRemovalMode : uint8
{
	/** Under budget: only old small sleeping pieces are removed, slowly. */
	Wear UMETA(DisplayName = "Wear"),

	/** Over budget: sleeping pieces are removed until the count is back under the budget. */
	Budget UMETA(DisplayName = "Budget"),

	/** Far over budget: sleeping pieces are removed quickly and in larger batches. */
	Emergency UMETA(DisplayName = "Emergency")
};

/** Snapshot of the physics budget counters, refreshed at every removal pass. */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_PhysicsBudgetStats
{
	GENERATED_BODY()

	/** Removal mode used by the last pass. */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	EWB_DebrisRemovalMode Mode = EWB_DebrisRemovalMode::Wear;

	/** Broken-off Geometry Collection pieces still in the simulation (shrinking pieces included). */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 ActivePieces = 0;

	/** Budget of active pieces (MaxDebrisPieces from the budget data). */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 MaxPieces = 0;

	/** Active pieces that are currently asleep. */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 SleepingPieces = 0;

	/** Pieces currently shrinking before being disabled. */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 ShrinkingPieces = 0;

	/** Total number of pieces removed since the manager started. */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 TotalRemovedPieces = 0;

	/** Number of Geometry Collections currently registered. */
	UPROPERTY(BlueprintReadOnly, Category = "Debris|Stats")
	int32 RegisteredCollections = 0;

	/** Number of physics props currently registered. */
	UPROPERTY(BlueprintReadOnly, Category = "Budget|Props")
	int32 RegisteredProps = 0;

	/** Registered physics props that are simulating physics. */
	UPROPERTY(BlueprintReadOnly, Category = "Budget|Props")
	int32 SimulatedProps = 0;

	/** Simulated bodies reserved for props (PhysicsPropsReserve from the budget data). Display only. */
	UPROPERTY(BlueprintReadOnly, Category = "Budget|Props")
	int32 PhysicsPropsReserve = 0;
};
