#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerDashProfile.generated.h"

/**
 * Dash settings of the player in one place (DA_PlayerDash_Default, lot P of Phase C). Read by BP_GenericCharacter (Dash, DashAnim,
 * ComputeDashImpactVelocity, ApplyDashImpact, TryDashHitPlayer) and by the bots, through UWB_PlayerCommandComponent::DashProfile
 * or UWB_PlayerProfileLibrary::GetPlayerDashProfile. The energy cost of the Dash is in UWB_PlayerEnergyProfile.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerDashProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Distance in cm a full Dash travels in front of the player. Also the range where the Dash looks for destructibles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashDistance = 350.f;

	/** Seconds a full Dash lasts (DashDistance / DashDuration = Dash speed). A Dash stopped early by a wall lasts its share of this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0.01", Units = "Seconds"))
	float DashDuration = 0.2f;

	/** Radius in cm of the sphere sweep that looks for destructibles along the Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashScanRadius = 25.f;

	/** Minimum run-up in cm before a destructible for the Dash to damage it (closer: no damage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashMinRunUpDistance = 35.f;

	/** Minimum run-up in cm before another player for the Dash to knock them Down (closer: nothing happens). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashMinRunUpDistancePlayer = 35.f;

	/** Angle in degrees between the Dash and the surface normal up to which the Dash deals full damage. Beyond, the damage falls toward DashMinAngleFactor at 90 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float DashFullDamageAngle = 30.f;

	/** Share of the damage kept by a grazing Dash (90 degrees to the surface normal), 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float DashMinAngleFactor = 0.85f;
};
