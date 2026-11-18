#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Chaos/ChaosEngineInterface.h"
#include "WB_SurfaceMovementProfile.generated.h"

/**
 * How one floor surface changes the player movement (lot 12.1): one asset per Physical Surface (DA_SurfaceMovement_Wood, _Tile, _Carpet, _Ice...).
 * Multipliers of the base values of the Character Movement, never stacked from one surface to the next. 1 = no change.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_SurfaceMovementProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Physical Surface of the floor Physical Material this profile applies to (Project Settings > Physics > Physical Surface). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	/** Top move speed on this floor. Applied as a move input scale combined with the state and possession multipliers, so values above 1 have no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxWalkSpeedMultiplier = 1.f;

	/** How fast the player reaches its speed (Character Movement MaxAcceleration). Low = slow start, like on ice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement", meta = (ClampMin = "0"))
	float MaxAccelerationMultiplier = 1.f;

	/** How fast the player stops when the stick is released (BrakingDecelerationWalking). Low = long slide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement", meta = (ClampMin = "0"))
	float BrakingDecelerationMultiplier = 1.f;

	/** Grip on the floor (GroundFriction): low = wide turns and slides, high = sharp turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement", meta = (ClampMin = "0"))
	float GroundFrictionMultiplier = 1.f;

	/** Dash distance on this floor (read by the Dash, lot 12.2). Above 1 = longer Dash, as on ice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement", meta = (ClampMin = "0"))
	float DashDistanceMultiplier = 1.f;
};

/**
 * All the floor surfaces known to the movement (DA_SurfaceMovementTable): the profile of the surface under the feet, or DefaultProfile.
 * Read by UWB_SurfaceMovementComponent.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_SurfaceMovementTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** One profile per floor surface. The first profile whose SurfaceType matches is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement")
	TArray<TObjectPtr<UWB_SurfaceMovementProfile>> Surfaces;

	/** Profile of any other surface (floor without Physical Material, or a surface missing from Surfaces). Empty: no change on those floors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceMovement")
	TObjectPtr<UWB_SurfaceMovementProfile> DefaultProfile;

	/** Profile for Surface: its entry in Surfaces, else DefaultProfile (can be null). */
	UFUNCTION(BlueprintPure, Category = "SurfaceMovement")
	const UWB_SurfaceMovementProfile* FindProfile(TEnumAsByte<EPhysicalSurface> Surface) const;
};
