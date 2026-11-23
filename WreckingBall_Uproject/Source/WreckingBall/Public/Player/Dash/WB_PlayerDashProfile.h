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
	// ---- Movement: weak Dash (tap) to full Dash (full charge) ----

	/** Distance in cm of a full Dash (full charge, or every Dash when bUseChargedDash is off). Also the range where the Dash looks for destructibles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashFullDistance = 350.f;

	/** Seconds a full Dash lasts (DashFullDistance / DashFullDuration = full Dash speed). A Dash stopped early by a wall lasts its share of this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0.01", Units = "Seconds"))
	float DashFullDuration = 0.2f;

	/** Distance in cm of a weak Dash (tap, no charge). Equal to DashFullDistance: the charge only changes the speed. Used when bUseChargedDash is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUseChargedDash"))
	float DashWeakDistance = 350.f;

	/** Seconds a weak Dash lasts (tap, no charge). Longer than DashFullDuration: the weak Dash is slower. Used when bUseChargedDash is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Movement", meta = (ClampMin = "0.01", Units = "Seconds", EditCondition = "bUseChargedDash"))
	float DashWeakDuration = 0.2f;

	// ---- Charge ----

	/**
	 * On: the Dash button charges the Dash. Press: the charge starts (the player keeps moving normally, the ground ring shows it); release: the Dash
	 * leaves, from the weak Dash (tap) to the full Dash (held DashChargeTime). The energy cost is the same whatever the charge.
	 * Off (default): the Dash leaves on the press, always full.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Charge")
	bool bUseChargedDash = false;

	/** Press shorter than this: weak Dash (no charge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Charge", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bUseChargedDash"))
	float DashChargeMinHoldTime = 0.12f;

	/** Hold time for a full charge. The charge rises linearly from DashChargeMinHoldTime to this; holding longer keeps it full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Charge", meta = (ClampMin = "0.01", Units = "Seconds", EditCondition = "bUseChargedDash"))
	float DashChargeTime = 0.5f;

	// ---- Impact ----

	/** Radius in cm of the sphere sweep that looks for destructibles along the Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashScanRadius = 25.f;

	/** Minimum run-up in cm before a destructible for the Dash to damage it (closer: no damage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashMinRunUpDistance = 35.f;

	/** Minimum run-up in cm before another player for the Dash to knock them Down (closer: nothing happens). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", Units = "Centimeters"))
	float DashMinRunUpDistancePlayer = 35.f;

	/** Angle in degrees between the Dash and the surface normal up to which the hit counts as head-on (no damage lost). Beyond, the damage falls toward DashGrazingDamageFactor at 90 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float DashHeadOnAngle = 30.f;

	/** Share of the damage kept by a grazing Dash (90 degrees to the surface normal), 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float DashGrazingDamageFactor = 0.85f;

	// ---- Damage ----

	/**
	 * On: the Dash damage to destructibles is linear (DashWeakDamage to DashFullDamage with the charge, x the angle factor), instead of the energy
	 * curve of DA_Physics_Dash. Balls and thrown objects keep the curve. Off (default): energy curve.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Damage")
	bool bUseChargedDashDamage = false;

	/** Head-on damage of a weak Dash (tap, no charge) to a destructible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Damage", meta = (ClampMin = "0", EditCondition = "bUseChargedDashDamage"))
	float DashWeakDamage = 13.f;

	/** Head-on damage of a full Dash (full charge) to a destructible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Damage", meta = (ClampMin = "0", EditCondition = "bUseChargedDashDamage"))
	float DashFullDamage = 19.f;

	// ---- Preview (editor readout: nothing here is read by the game) ----

	/** Simulation input, never read by the game: health of the simulated wall (MaxHealth x HealthMultiplier of a placed wall). Walls of the project: 36. */
	UPROPERTY(EditAnywhere, Category = "Dash|Preview|1 - Simulation Inputs", meta = (ClampMin = "1"))
	float SimulatedWallHealth = 36.f;

	/** Simulation input, never read by the game: MinDamagePerHit of the simulated wall. A Dash under it does nothing. Walls of the project: 10. */
	UPROPERTY(EditAnywhere, Category = "Dash|Preview|1 - Simulation Inputs", meta = (ClampMin = "0"))
	float SimulatedWallMinDamage = 10.f;

	/** Simulation input, never read by the game: angle in degrees between the Dash and the wall normal for the "at angle" rows (0: head-on, 90: grazing). */
	UPROPERTY(EditAnywhere, Category = "Dash|Preview|1 - Simulation Inputs", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float SimulatedDashAngle = 60.f;

	/** Speed in cm/s of a weak Dash (DashWeakDistance / DashWeakDuration). Read-only, refreshed on each edit. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewWeakSpeed = 0.f;

	/** Speed in cm/s of a full Dash (DashFullDistance / DashFullDuration). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewFullSpeed = 0.f;

	/** Damage share kept at SimulatedDashAngle (1 up to DashHeadOnAngle, DashGrazingDamageFactor at 90 degrees). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewAngleFactor = 0.f;

	/** Damage of a weak Dash, head-on (DashWeakDamage). Read-only. Linear damage only (bUseChargedDashDamage). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewWeakDamageHeadOn = 0.f;

	/** Damage of a full Dash, head-on (DashFullDamage). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewFullDamageHeadOn = 0.f;

	/** Damage of a weak Dash at SimulatedDashAngle. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewWeakDamageAtAngle = 0.f;

	/** Damage of a full Dash at SimulatedDashAngle. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewFullDamageAtAngle = 0.f;

	/** Damage of a weak Dash, grazing (90 degrees: DashWeakDamage x DashGrazingDamageFactor). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewWeakDamageGrazing = 0.f;

	/** Damage of a full Dash, grazing (90 degrees). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|3 - Damage")
	float PreviewFullDamageGrazing = 0.f;

	/** Dashes to break the simulated wall with full Dashes, head-on (best case). A wall never breaks on the first hit (MaxStatesPerHit 1). 0: never (under SimulatedWallMinDamage). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|4 - Hits to Break the Wall")
	int32 PreviewHitsFullHeadOn = 0;

	/** Dashes to break the simulated wall with full Dashes at SimulatedDashAngle. 0: never. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|4 - Hits to Break the Wall")
	int32 PreviewHitsFullAtAngle = 0;

	/** Dashes to break the simulated wall with weak Dashes, head-on. 0: never. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|4 - Hits to Break the Wall")
	int32 PreviewHitsWeakHeadOn = 0;

	/** Dashes to break the simulated wall with weak Dashes at SimulatedDashAngle. 0: never. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|4 - Hits to Break the Wall")
	int32 PreviewHitsWeakAtAngle = 0;

	/** Dashes to break the simulated wall with weak Dashes, grazing (worst case). 0: never. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Dash|Preview|4 - Hits to Break the Wall")
	int32 PreviewHitsWeakGrazing = 0;

	/** Damage share kept at Angle degrees from the surface normal: 1 up to DashHeadOnAngle, then down to DashGrazingDamageFactor at 90 (same as ComputeDashImpactVelocity). */
	UFUNCTION(BlueprintPure, Category = "Dash|Impact")
	float GetAngleFactor(float Angle) const;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Charge (0..1) of a Dash button held HoldTime seconds. Always 1 when bUseChargedDash is off. */
	UFUNCTION(BlueprintPure, Category = "Dash|Charge")
	float GetChargeFromHoldTime(float HoldTime) const;

	/** Distance in cm of a Dash at Charge (0..1): DashWeakDistance to DashFullDistance. DashFullDistance when bUseChargedDash is off. */
	UFUNCTION(BlueprintPure, Category = "Dash|Movement")
	float GetDistanceForCharge(float Charge) const;

	/** Duration in seconds of a Dash at Charge (0..1): DashWeakDuration to DashFullDuration. DashFullDuration when bUseChargedDash is off. */
	UFUNCTION(BlueprintPure, Category = "Dash|Movement")
	float GetDurationForCharge(float Charge) const;

	/** Head-on damage to a destructible of a Dash at Charge (0..1): DashWeakDamage to DashFullDamage. */
	UFUNCTION(BlueprintPure, Category = "Dash|Damage")
	float GetDamageForCharge(float Charge) const;

private:
	/** Fills the Preview fields from the settings. */
	void RefreshPreview();

	/** Dashes of Damage to break the simulated wall (SimulatedWallHealth), the first hit leaving at least 1 health. 0 under SimulatedWallMinDamage. */
	int32 ComputePreviewHits(float Damage) const;
};
