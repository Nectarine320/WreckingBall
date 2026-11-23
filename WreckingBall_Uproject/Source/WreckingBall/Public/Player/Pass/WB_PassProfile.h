#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PassProfile.generated.h"

/** Where a pass is aimed. Fixed choice of the pass computation (devis match 3v3, lot A). */
UENUM(BlueprintType)
enum class EWB_PassAimMode : uint8
{
	/** Version A: aims where the receiver will be when the ball arrives (its current velocity, up to MaxLeadTime). */
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
	/** Lowest pass speed, as a share of the ball's max kick speed (0.4 = 40 % of the kick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0", ClampMax = "1"))
	float MinKickShare = 0.4f;

	/** Speed in cm/s the ball should still have when it reaches the receiver. Raises the pass speed for far receivers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ArrivalSpeed = 400.f;

	/** How fast a rolling ball slows down, in cm/s per second (floor friction and damping, measured in game). Raises the pass speed for far receivers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Speed", meta = (ClampMin = "0"))
	float RollingDeceleration = 500.f;

	/** Where the pass is aimed: Lead (version A, anticipated position) or Direct (version B, test). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim")
	EWB_PassAimMode PassAimMode = EWB_PassAimMode::Lead;

	/** Lead mode: the receiver's movement is anticipated for at most this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", Units = "Seconds"))
	float MaxLeadTime = 1.f;

	/**
	 * On: aim assist. The pass goes to the teammate pointed by the stick only when it stands within AimAssistAngle of the stick direction
	 * (stick at rest: the player's facing); else it is a free pass along that direction, with no receiver. Off (default): the pass always goes
	 * to the teammate best aligned with the stick, whatever the angle (nearest teammate when the stick is at rest).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim")
	bool bUseAimAssist = false;

	/** Aim assist: half-angle in degrees of the cone around the stick direction in which a teammate gets the pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees", EditCondition = "bUseAimAssist"))
	float AimAssistAngle = 15.f;

	/** Aim assist: a free pass (no teammate in the cone) gets the speed of a pass to a receiver this many cm away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Aim", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUseAimAssist"))
	float FreePassDistance = 800.f;

	// ---- Charged pass ----

	/**
	 * On: the pass button charges the pass. Press with the ball: the charge starts (the player keeps dribbling); release: the pass leaves,
	 * faster the longer the button was held (up to ChargedPassMaxSpeedMultiplier), along the stick at release. Off (default): the pass leaves on the press.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge")
	bool bUseChargedPass = false;

	/** Press shorter than this: plain pass, no charge (a tap stays a normal pass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bUseChargedPass"))
	float ChargeMinHoldTime = 0.12f;

	/** Hold time for a full charge. The charge rises linearly from ChargeMinHoldTime to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0.01", Units = "Seconds", EditCondition = "bUseChargedPass"))
	float PassChargeTime = 0.6f;

	/** Pass speed multiplier at full charge (1 at no charge, linear in between). Still capped at the ball's max kick speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "1", EditCondition = "bUseChargedPass"))
	float ChargedPassMaxSpeedMultiplier = 1.8f;

	/** Strong pass only (charge at or above PassLoftMinCharge): the ball leaves with a random lift between 0 and this angle. 0: always on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", ClampMax = "5", Units = "Degrees", EditCondition = "bUseChargedPass"))
	float PassMaxLoftAngle = 5.f;

	/** Charge (0..1) from which a pass may lift (PassMaxLoftAngle). Under it, the pass stays on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Charge", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bUseChargedPass"))
	float PassLoftMinCharge = 0.5f;

	/** Charge (0..1) of a pass button held HoldTime seconds: 0 under ChargeMinHoldTime, 1 at PassChargeTime. Always 0 when bUseChargedPass is off. */
	UFUNCTION(BlueprintPure, Category = "Pass|Charge")
	float GetChargeFromHoldTime(float HoldTime) const;

	/** Pass speed multiplier for Charge (0..1): 1 to ChargedPassMaxSpeedMultiplier. */
	UFUNCTION(BlueprintPure, Category = "Pass|Charge")
	float GetChargeSpeedMultiplier(float Charge) const;

	/** Adds the strong pass lift to a flat pass velocity: a random angle up to PassMaxLoftAngle when Charge >= PassLoftMinCharge; unchanged otherwise. */
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
};
