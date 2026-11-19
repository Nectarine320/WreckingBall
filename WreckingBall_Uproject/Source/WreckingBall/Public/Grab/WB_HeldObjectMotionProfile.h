#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_HeldObjectMotionProfile.generated.h"

/**
 * How a held object moves in front of the player (DA_HeldObjectMotion_Default, UI 3D devis, lot A): carry point, sway, lag, throw arc.
 * Read every frame by UWB_HeldObjectMotionComponent, which drives the target of the character's Physics Handle.
 * Replaces the per-call constants of BPC_GrabObject (UpdatePhysicActorPosition). Speeds are per second, so the motion is the same at any frame rate;
 * the default values match the old look at 60 FPS.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_HeldObjectMotionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Carry point ----

	/** Distance in cm from the capsule axis to the held object, in front of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Carry", meta = (ClampMin = "0", Units = "Centimeters"))
	float CarryDistance = 125.f;

	/** Height in cm of a held ball above the bottom of the capsule (the feet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Carry", meta = (Units = "Centimeters"))
	float BallCarryHeight = 10.f;

	/** Height in cm of any other held object above the bottom of the capsule (the feet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Carry", meta = (Units = "Centimeters"))
	float ObjectCarryHeight = 75.f;

	// ---- Position sway ----

	/** Radius in cm of the small circle the held object sways along (side and up). 0: no position sway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway", meta = (ClampMin = "0", Units = "Centimeters"))
	float PositionSwayRadius = 1.1f;

	/** Position sway speed in degrees per second when the player stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway", meta = (ClampMin = "0"))
	float PositionSwaySpeedIdle = 13.3f;

	/** Position sway speed in degrees per second at full walk speed (blends from the idle speed with the player's speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway", meta = (ClampMin = "0"))
	float PositionSwaySpeedMoving = 25.f;

	// ---- Rotation sway ----

	/** Largest rotation sway in degrees on each axis. Roll and Pitch follow a sine, Yaw a cosine. 0 on an axis: no sway on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway")
	FRotator RotationSwayAmplitude = FRotator(9.f, 5.f, 7.f);

	/** Rotation sway speed in degrees per second when the player stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway", meta = (ClampMin = "0"))
	float RotationSwaySpeedIdle = 4.2f;

	/** Rotation sway speed in degrees per second at full walk speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Sway", meta = (ClampMin = "0"))
	float RotationSwaySpeedMoving = 12.5f;

	// ---- Velocity lag ----

	/** The held object trails behind sideways and vertically by the player's velocity (cm/s) x this x the mass factor below. 0: no lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Lag", meta = (ClampMin = "0"))
	float VelocityLagMultiplier = 0.0001f;

	/** Mass in kg at and below which the light mass factor applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Lag", meta = (ClampMin = "0", Units = "Kilograms"))
	float LagLightMass = 100.f;

	/** Mass in kg at and above which the heavy mass factor applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Lag", meta = (ClampMin = "0", Units = "Kilograms"))
	float LagHeavyMass = 300.f;

	/** Velocity lag mass factor of a light object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Lag", meta = (ClampMin = "0"))
	float LagLightMassFactor = 300.f;

	/** Velocity lag mass factor of a heavy object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Lag", meta = (ClampMin = "0"))
	float LagHeavyMassFactor = 100.f;

	// ---- Weight sag (spring) ----

	/** The held object sags toward the player's right and up by its mass (kg) x this, in cm (negative: left and down). A spring follows that offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Weight")
	float WeightOffsetPerKg = -0.0216f;

	/** Frequency in Hz of the spring that follows the weight offset: higher follows faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Weight", meta = (ClampMin = "0.01"))
	float WeightSpringFrequency = 2.65f;

	/** Damping of the weight spring: 1 = no bounce, lower bounces more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Weight", meta = (ClampMin = "0", ClampMax = "2"))
	float WeightSpringDamping = 0.3f;

	// ---- Velocity tilt ----

	/** On: the held object tilts with the player's velocity (roll from the side speed, pitch from the forward speed). Off (default): no tilt, as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Tilt")
	bool bEnableVelocityTilt = false;

	/** Tilt in degrees at full walk speed (negative tilts the other way). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Tilt", meta = (EditCondition = "bEnableVelocityTilt"))
	float MaxTiltAngle = -12.f;

	/** How fast the tilt follows the velocity, per second: higher follows faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|Tilt", meta = (ClampMin = "0", EditCondition = "bEnableVelocityTilt"))
	float TiltFollowSpeed = 33.7f;

	// ---- Throw arc ----

	/** A held object whose bounding sphere radius (cm) reaches this is thrown with both hands: during the throw windup it travels on an arc over the head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|ThrowArc", meta = (ClampMin = "0", Units = "Centimeters"))
	float ThrowArcMinBoundsRadius = 30.f;

	/** Height in cm added to the middle of the windup arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|ThrowArc", meta = (Units = "Centimeters"))
	float ThrowArcHeight = 25.f;

	/** Height in cm of the end of the windup above the top of the head (plus half the object). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|ThrowArc", meta = (Units = "Centimeters"))
	float ThrowOverheadHeight = 15.f;

	/** Distance in cm of the end of the windup in front of the capsule axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|ThrowArc", meta = (Units = "Centimeters"))
	float ThrowOverheadForward = 10.f;

	/** Gap in cm kept between the object and the capsule during the windup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeldObject|ThrowArc", meta = (ClampMin = "0", Units = "Centimeters"))
	float ThrowClearanceMargin = 5.f;
};
