#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "WB_BallImpactScanProfile.generated.h"

/**
 * Settings of the impact scan of a kicked ball (DA_BallImpactScan_Default), read by UWB_BallImpactScanComponent.
 * While the ball goes fast, a sphere is swept ahead of it along its velocity; every blocking hit is reported to the ball Blueprint,
 * which applies the impact (BPI_ImpactReceiver) before the physics contact happens.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_BallImpactScanProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Speed of the ball (cm/s) above which the scan runs. Under it, the scan stops until the next kick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MinSpeed = 1300.f;

	/** How far ahead the sphere is swept, in seconds of travel: the sweep length is the ball speed times this value (0.05 s at 2000 cm/s = 100 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan", meta = (ClampMin = "0", Units = "Seconds"))
	float LookAheadTime = 0.05f;

	/** Radius of the swept sphere, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan", meta = (ClampMin = "0", Units = "Centimeters"))
	float ScanRadius = 25.f;

	/** Time between two scans, in seconds. Shorter = fewer impacts missed by a very fast ball, but more sweeps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan", meta = (ClampMin = "0.01", Units = "Seconds"))
	float ScanInterval = 0.033f;

	/** Trace channel of the sweep. Visibility matches the former Blueprint scan (TraceTypeQuery1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Draws each sweep (green) and its blocking hits (red) for ScanInterval seconds. Debug only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BallImpactScan|Debug")
	bool bDrawDebug = false;
};
