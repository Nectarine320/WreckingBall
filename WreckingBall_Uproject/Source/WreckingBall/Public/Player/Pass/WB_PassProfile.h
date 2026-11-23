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

	/** Ball function (Blueprint interface) that returns its physics profile (PDA_PhysicsProfile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Readout")
	FName PhysicsProfileFunction = TEXT("GetPhysicsProfile");

	/** Number variable of the physics profile holding the max kick speed in cm/s. 0 or less: no cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pass|Readout")
	FName MaxKickSpeedVariable = TEXT("MaxKickSpeed");

	/**
	 * Flat ball velocity (cm/s) of a pass from BallLocation to a receiver at ReceiverLocation moving at ReceiverVelocity.
	 * MaxKickSpeed: max kick speed of the ball (0 or less: no cap, and no minimum share).
	 * Returns false when the receiver is on the ball (no direction).
	 */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Valid"))
	bool ComputePassVelocity(FVector BallLocation, FVector ReceiverLocation, FVector ReceiverVelocity, float MaxKickSpeed, FVector& OutVelocity) const;

	/** Flat ball velocity (cm/s) of a free pass along Direction: the speed of a pass over FreePassDistance. False when Direction is zero. */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Valid"))
	bool ComputeFreePassVelocity(FVector Direction, float MaxKickSpeed, FVector& OutVelocity) const;

	/** Max kick speed of Ball, read from its physics profile (PhysicsProfileFunction, MaxKickSpeedVariable). Returns false, with a warning, when it cannot be read. */
	UFUNCTION(BlueprintPure, Category = "Pass", meta = (ReturnDisplayName = "Found"))
	bool ReadBallMaxKickSpeed(const AActor* Ball, float& OutMaxKickSpeed) const;

private:
	/** Pass speed for a ball travelling Distance cm. */
	float GetPassSpeed(float Distance, float MaxKickSpeed) const;
};
