#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PassProfile.generated.h"

/** Where a pass is aimed. Fixed choice of the pass computation (devis match 3v3, lot A). */
UENUM(BlueprintType)
enum class EWB_PassAimMode : uint8
{
	/** Version A: aims where the receiver will be when the ball arrives (its current velocity, up to PassMaxLeadTime). */
	Lead,
	/** Version B (test): aims at the receiver's current position. */
	Direct
};

/**
 * Pass settings (devis match 3v3, lot A): ground pass toward a teammate, speed from a share of the ball's kick, raised when the
 * receiver is far, capped at the kick. Asset: DA_Pass_Default, set on the UWB_PlayerCommandComponent of BP_GenericCharacter.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PassProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Speed ----

	/** Lowest pass speed, as a share of the ball's max kick speed (0.4 = 40 % of the kick). No effect on a ball without a kick cap (MaxKickSpeed 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0", ClampMax = "1"))
	float PassMinShareOfMaxKickSpeed = 0.4f;

	/** Speed in cm/s the ball should still have when it reaches the receiver. Raises the pass speed for far receivers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float PassArrivalSpeed = 400.f;

	/** How fast a rolling ball slows down, in cm/s per second (floor friction and damping, measured in game). Raises the pass speed for far receivers. Also read by the receiving bots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0"))
	float PassRollingDeceleration = 500.f;

	// ---- Aim ----

	/** Where the pass is aimed: Lead (version A, anticipated position) or Direct (version B, test). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim")
	EWB_PassAimMode PassAimMode = EWB_PassAimMode::Lead;

	/** Lead mode: the receiver's movement is anticipated for at most this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", Units = "Seconds"))
	float PassMaxLeadTime = 1.f;

	/**
	 * On: aim assist. The pass goes to the teammate pointed by the stick only when it stands within PassAimAssistAngle of the stick direction
	 * (stick at rest: the player's facing); else it is a free pass along that direction, with no receiver. Off (default): the pass always goes
	 * to the teammate best aligned with the stick, whatever the angle (nearest teammate when the stick is at rest).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim")
	bool bUsePassAimAssist = false;

	/** Aim assist: half-angle in degrees of the cone around the stick direction in which a teammate gets the pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees", EditCondition = "bUsePassAimAssist"))
	float PassAimAssistAngle = 15.f;

	/** Aim assist: a free pass (no teammate in the cone) gets the speed of a pass to a receiver this many cm away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUsePassAimAssist"))
	float FreePassDistance = 800.f;

	// ---- Charge: weak pass (tap) to full pass (full charge) ----

	/**
	 * On: the pass button charges the pass. Press with the ball: the charge starts (the player keeps dribbling, the ground ring shows it);
	 * release: the pass leaves along the stick, from the weak pass (tap) to the full pass (held PassChargeTime). Off (default): the pass leaves on the press.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge")
	bool bUseChargedPass = false;

	/** Press shorter than this: weak pass (no charge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bUseChargedPass"))
	float PassChargeMinHoldTime = 0.12f;

	/** Hold time for a full charge. The charge rises linearly from PassChargeMinHoldTime to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0.01", Units = "Seconds", EditCondition = "bUseChargedPass"))
	float PassChargeTime = 0.6f;

	/** Pass speed multiplier of a full pass (the weak pass is x1, linear in between). Still capped at the ball's max kick speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "1", EditCondition = "bUseChargedPass"))
	float PassFullChargeSpeedMultiplier = 1.8f;

	/** Strong pass only (charge at or above PassLoftMinCharge): the ball leaves with a random lift between 0 and this angle. 0: always on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", ClampMax = "5", Units = "Degrees", EditCondition = "bUseChargedPass"))
	float PassFullChargeMaxLoftAngle = 5.f;

	/** Charge (0..1) from which a pass may lift (PassFullChargeMaxLoftAngle). Under it, the pass stays on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bUseChargedPass"))
	float PassLoftMinCharge = 0.5f;

	// ---- Preview (editor readout: nothing here is read by the game) ----

	/** Simulation input, never read by the game: ball whose max kick speed caps the simulated passes (a DA_BallPhysics_*). Empty: no cap. */
	UPROPERTY(EditAnywhere, Category = "Pass|Preview|1 - Simulation Inputs")
	TObjectPtr<UPrimaryDataAsset> SimulatedBallPhysics;

	/** Simulation input, never read by the game: distance in cm from the ball to the simulated receiver. */
	UPROPERTY(EditAnywhere, Category = "Pass|Preview|1 - Simulation Inputs", meta = (ClampMin = "0", Units = "Centimeters"))
	float SimulatedPassDistance = 800.f;

	/** Max kick speed read from SimulatedBallPhysics (MaxKickSpeedVariable). 0: no cap, and PassMinShareOfMaxKickSpeed has no effect. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewBallMaxKickSpeed = 0.f;

	/** Start speed of a weak pass (tap) to the simulated receiver. Read-only, refreshed on each edit. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewWeakPassSpeed = 0.f;

	/** Start speed of a full pass (full charge) to the simulated receiver. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewFullPassSpeed = 0.f;

	/** True when the full pass is cut by the ball's max kick speed (the charge stops adding speed). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed")
	bool bPreviewFullPassCapped = false;

	/** Start speed of a weak free pass (no receiver, FreePassDistance). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewWeakFreePassSpeed = 0.f;

	/** Start speed of a full free pass. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|2 - Speed", meta = (Units = "CentimetersPerSecond"))
	float PreviewFullFreePassSpeed = 0.f;

	/** Seconds a weak pass takes to reach the simulated receiver (rolling, PassRollingDeceleration). 0: the ball stops before. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|3 - Travel", meta = (Units = "Seconds"))
	float PreviewWeakTravelTime = 0.f;

	/** Seconds a full pass takes to reach the simulated receiver. 0: the ball stops before. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|3 - Travel", meta = (Units = "Seconds"))
	float PreviewFullTravelTime = 0.f;

	/** Speed of a weak pass when it reaches the simulated receiver. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|3 - Travel", meta = (Units = "CentimetersPerSecond"))
	float PreviewWeakArrivalSpeed = 0.f;

	/** Speed of a full pass when it reaches the simulated receiver. Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|3 - Travel", meta = (Units = "CentimetersPerSecond"))
	float PreviewFullArrivalSpeed = 0.f;

	/** Seconds the button must be held before the pass may lift (PassLoftMinCharge). Read-only. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Pass|Preview|4 - Charge and Loft", meta = (Units = "Seconds"))
	float PreviewLoftStartHoldTime = 0.f;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Charge (0..1) of a pass button held HoldTime seconds: 0 under PassChargeMinHoldTime, 1 at PassChargeTime. Always 0 when bUseChargedPass is off. */
	UFUNCTION(BlueprintPure, Category = "Pass|Charge")
	float GetChargeFromHoldTime(float HoldTime) const;

	/** Pass speed multiplier for Charge (0..1): 1 to PassFullChargeSpeedMultiplier. */
	UFUNCTION(BlueprintPure, Category = "Pass|Charge")
	float GetChargeSpeedMultiplier(float Charge) const;

	/** Adds the strong pass lift to a flat pass velocity: a random angle up to PassFullChargeMaxLoftAngle when Charge >= PassLoftMinCharge; unchanged otherwise. */
	UFUNCTION(BlueprintPure, Category = "Pass|Charge")
	FVector ApplyPassLoft(FVector FlatVelocity, float Charge) const;

	/** Ball function (Blueprint interface) that returns its physics profile (PDA_PhysicsProfile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Readout")
	FName PhysicsProfileFunction = TEXT("GetPhysicsProfile");

	/** Number variable of the physics profile holding the max kick speed in cm/s. 0 or less: no cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Readout")
	FName MaxKickSpeedVariable = TEXT("MaxKickSpeed");

	/**
	 * Flat ball velocity (cm/s) of a pass from BallLocation to a receiver at ReceiverLocation moving at ReceiverVelocity.
	 * MaxKickSpeed: max kick speed of the ball (0 or less: no cap, and no minimum share). SpeedMultiplier: charged pass (GetChargeSpeedMultiplier), 1 for a plain pass.
	 * Returns false when the receiver is on the ball (no direction).
	 */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Valid"))
	bool ComputePassVelocity(FVector BallLocation, FVector ReceiverLocation, FVector ReceiverVelocity, float MaxKickSpeed, FVector& OutVelocity, float SpeedMultiplier = 1.f) const;

	/** Flat ball velocity (cm/s) of a free pass along Direction: the speed of a pass over FreePassDistance, x SpeedMultiplier. False when Direction is zero. */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Valid"))
	bool ComputeFreePassVelocity(FVector Direction, float MaxKickSpeed, FVector& OutVelocity, float SpeedMultiplier = 1.f) const;

	/** Max kick speed of Ball, read from its physics profile (PhysicsProfileFunction, MaxKickSpeedVariable). Returns false, with a warning, when it cannot be read. */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Found"))
	bool ReadBallMaxKickSpeed(const AActor* Ball, float& OutMaxKickSpeed) const;

private:
	/** Pass speed for a ball travelling Distance cm, x SpeedMultiplier, capped at MaxKickSpeed. */
	float GetPassSpeed(float Distance, float MaxKickSpeed, float SpeedMultiplier = 1.f) const;

	/** Fills the Preview fields from the settings. */
	void RefreshPreview();

	/** Rolling from Speed over Distance: seconds to get there and speed left (0 and 0 when the ball stops before). */
	void ComputeTravel(float Speed, float Distance, float& OutTime, float& OutArrivalSpeed) const;
};
