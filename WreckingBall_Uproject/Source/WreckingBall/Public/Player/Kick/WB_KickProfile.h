#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_KickProfile.generated.h"

/**
 * Kick and throw charge settings of the player (DA_Kick_Default, set on the UWB_PlayerCommandComponent of BP_GenericCharacter).
 * Read by BPC_HeldObjectActions: the charge rises from the weak kick (tap) to the full kick (held KickChargeTime), and the kick impulse is
 * the ball's KickImpulse x the strength of the charge, capped at the ball's MaxKickSpeed (both in the ball's PDA_PhysicsProfile).
 * The throw of a held object uses the same charge. The move speed while charging and the state durations are in DA_PlayerState_Default.
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Player"))
class WRECKINGBALL_API UWB_KickProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Charge: weak kick (tap) to full kick (full charge) ----

	/** Strength of a weak kick (tap, no charge): the ball's KickImpulse is multiplied by this. Also the weak throw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kick|Charge", meta = (ClampMin = "0"))
	float KickWeakStrength = 1.f;

	/** Strength of a full kick (full charge): the ball's KickImpulse is multiplied by this. Also the full throw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kick|Charge", meta = (ClampMin = "0"))
	float KickFullStrength = 4.f;

	/** Seconds of hold for a full charge (the ground ring fills over this time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kick|Charge", meta = (ClampMin = "0.01", Units = "Seconds"))
	float KickChargeTime = 0.66f;

	/** Kick strength at Charge (0..1): KickWeakStrength to KickFullStrength. */
	UFUNCTION(BlueprintPure, Category = "Kick|Charge")
	float GetStrengthForCharge(float Charge) const;

	/** Charge (0..1) gained in TickSeconds of hold: TickSeconds / KickChargeTime. Read by the charge timer of BPC_HeldObjectActions. */
	UFUNCTION(BlueprintPure, Category = "Kick|Charge")
	float GetChargeStep(float TickSeconds) const;

	// ---- Preview (editor readout: nothing here is read by the game) ----

	/** Simulation input, never read by the game: ball kicked in the preview (a DA_BallPhysics_*: mass, KickImpulse, MaxKickSpeed, loft, damage). */
	UPROPERTY(EditAnywhere, Category = "Kick|Preview|1 - Simulation Inputs")
	TObjectPtr<UPrimaryDataAsset> SimulatedBallPhysics;

	/** Simulation input, never read by the game: wall hit head-on by the kicked ball (a destructible profile: MaxHealth, MinDamagePerHit, energy curve). */
	UPROPERTY(EditAnywhere, Category = "Kick|Preview|1 - Simulation Inputs")
	TObjectPtr<UPrimaryDataAsset> SimulatedWall;

	/** Mass in kg read from SimulatedBallPhysics. Read-only, refreshed on each edit of this asset (or when it opens). */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|2 - Read From the Simulated Assets")
	float PreviewBallMassKg = 0.f;

	/** KickImpulse read from SimulatedBallPhysics (impulse of a x1 kick). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|2 - Read From the Simulated Assets")
	float PreviewBallKickImpulse = 0.f;

	/** MaxKickSpeed read from SimulatedBallPhysics, in cm/s. 0: no cap. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|2 - Read From the Simulated Assets", meta = (Units = "CentimetersPerSecond"))
	float PreviewBallMaxKickSpeed = 0.f;

	/** MaxHealth read from SimulatedWall. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|2 - Read From the Simulated Assets")
	float PreviewWallHealth = 0.f;

	/** MinDamagePerHit read from SimulatedWall. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|2 - Read From the Simulated Assets")
	float PreviewWallMinDamage = 0.f;

	/** Speed in cm/s of the ball after a weak kick (tap). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|3 - Speed and Loft", meta = (Units = "CentimetersPerSecond"))
	float PreviewWeakKickSpeed = 0.f;

	/** Speed in cm/s of the ball after a full kick. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|3 - Speed and Loft", meta = (Units = "CentimetersPerSecond"))
	float PreviewFullKickSpeed = 0.f;

	/** True when the full kick is cut by the ball's MaxKickSpeed (more strength adds nothing). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|3 - Speed and Loft")
	bool bPreviewFullKickCapped = false;

	/** Highest lift of a kick, in degrees (the ball's KickMaxLoftAngle; each kick draws 0 to this). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|3 - Speed and Loft", meta = (Units = "Degrees"))
	float PreviewMaxLoftAngle = 0.f;

	/** Impact energy (J) of a weak kick hitting the wall head-on at its kick speed. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|4 - Damage")
	float PreviewWeakEnergy = 0.f;

	/** Impact energy (J) of a full kick hitting the wall head-on. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|4 - Damage")
	float PreviewFullEnergy = 0.f;

	/** Damage of a weak kick on the simulated wall (energy curve x the ball's DamageMultiplier; 0 under the ball's MinImpactSpeed or the wall's MinDamagePerHit). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|4 - Damage")
	float PreviewWeakDamage = 0.f;

	/** Damage of a full kick on the simulated wall. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|4 - Damage")
	float PreviewFullDamage = 0.f;

	/** Weak kicks to break the simulated wall head-on (0: never). The first hit leaves 1 health when the ball's MaxStatesPerHit is under 2. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|5 - Hits to Break the Wall")
	int32 PreviewHitsWeak = 0;

	/** Full kicks to break the simulated wall head-on (0: never). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Kick|Preview|5 - Hits to Break the Wall")
	int32 PreviewHitsFull = 0;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Fills the Preview fields from the settings and the simulated assets. */
	void RefreshPreview();

	/** Ball speed after a kick of Strength (impulse / mass, capped at MaxKickSpeed). */
	float ComputeKickSpeed(float Strength, float& OutUncappedSpeed) const;

	/** Damage of the simulated ball hitting the simulated wall head-on at Speed, and its energy. */
	float ComputeWallDamage(float Speed, float& OutEnergy) const;

	/** Hits of Damage to break the simulated wall. 0 when Damage is 0. */
	int32 ComputeHits(float Damage) const;
};
