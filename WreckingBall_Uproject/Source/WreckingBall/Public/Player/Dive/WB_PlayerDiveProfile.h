#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerDiveProfile.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;

/**
 * Tuning of the defensive dive (UWB_PlayerDiveComponent, DA_PlayerDive_Default). Distance, speed and height are balanced often:
 * Docs/Plans/plan-plongeon.md lists every value and what it changes. Energy costs are in UWB_PlayerActionCostProfile.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerDiveProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Target ----

	/** Ball actors the dive and the defense look for (BP_PO_Master_Ball_01 and its children). Empty: the dive always goes straight along the stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Target")
	TSubclassOf<AActor> BallClass;

	/** Range in cm around the player: a ball within it (kicked or not) is aimed at by the dive when it is in the stick cone. Farther: ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Target", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxBallDistance = 1200.f;

	// ---- Jump ----

	/** Seconds in the air, from takeoff to touchdown. The landing and the get-up come after. Main speed setting of the dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0.2", Units = "Seconds"))
	float AirDuration = 0.375f;

	/** Shortest dive in cm (flat distance from takeoff to touchdown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MinDistance = 200.f;

	/** Longest dive in cm. Also the distance of a dive straight along the stick (no ball in range or in the stick cone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxDistance = 800.f;

	/** Lowest height in cm above the ground of the body at the top of the dive (a low dive along the floor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MinHeight = 50.f;

	/** Highest height in cm above the ground of the body at the top of the dive. The dive aims at the ball's height, clamped between MinHeight and this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxHeight = 225.f;

	/**
	 * Height in cm above the ground of the body at the top of a dive straight along the stick (no ball in the cone). Above the standing height
	 * (about 90), the dive draws an arc: fast rise, top, then a fall that speeds up. A dive aimed at a ball uses the ball's height instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float FreeDiveHeight = 140.f;

	// ---- Aim ----

	/** Half angle in degrees of the cone around the stick direction where the dive looks for the ball path (30 = a 60 degree cone). Ball in the cone: the dive goes toward it; outside: straight along the stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Aim", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float StickConeHalfAngle = 30.f;

	/** How far in cm the body center may pass from the predicted ball and still count the point as reachable (body length and arms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Aim", meta = (ClampMin = "0", Units = "Centimeters"))
	float PlanningReach = 70.f;

	/** Earliest moment of the jump, as a fraction of AirDuration, where the body may meet the ball (the jump needs a moment to take off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Aim", meta = (ClampMin = "0", ClampMax = "1"))
	float MinInterceptFraction = 0.2f;

	/** Latest moment of the jump, as a fraction of AirDuration, where the body may meet the ball (it is falling to the ground after). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Aim", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxInterceptFraction = 0.85f;

	// ---- Body ----

	/** Tilt of the body in degrees toward the jump direction while in the air (90 = lying flat). The player keeps facing the ball, so a side dive lies on its side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Body", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float BodyTilt = 80.f;

	/** Seconds the body takes to tilt at takeoff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Body", meta = (ClampMin = "0.01", Units = "Seconds"))
	float BodyTiltInTime = 0.2f;

	/** Height in cm above the ground of the body center lying on the floor after the touchdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Body", meta = (ClampMin = "0", Units = "Centimeters"))
	float LyingHeight = 25.f;

	/** Radius in cm of the body used to detect the ball touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Body", meta = (ClampMin = "0", Units = "Centimeters"))
	float BodyRadius = 25.f;

	/** Length in cm the stretched arms add beyond the head, for the ball touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Body", meta = (ClampMin = "0", Units = "Centimeters"))
	float ArmReach = 45.f;

	// ---- Ball ----

	/** Share of the ball speed kept when it bounces off the diver or the defense stance (0..1), away from the body like on a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Ball", meta = (ClampMin = "0", ClampMax = "1"))
	float BallBounceRestitution = 0.6f;

	// ---- Defense ----

	/** Speed of W/S (forward / back, toward or away from the ball) in the defense stance, as a share of the side-step speed of A/D. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Defense", meta = (ClampMin = "0", ClampMax = "1"))
	float DefenseForwardSpeedScale = 0.5f;

	/** Half width in cm of the blocking body in the defense stance (arms and legs spread in an X), sideways from the body axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Defense", meta = (ClampMin = "0", Units = "Centimeters"))
	float DefenseBlockHalfWidth = 70.f;

	/** Half thickness in cm of the blocking body in the defense stance, front to back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Defense", meta = (ClampMin = "0", Units = "Centimeters"))
	float DefenseBlockHalfThickness = 25.f;

	/** Height in cm the raised hands add above the capsule top in the defense stance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Defense", meta = (ClampMin = "0", Units = "Centimeters"))
	float DefenseBlockExtraHeight = 20.f;

	// ---- Landing ----

	/** Seconds the player stays on the ground after a dive that missed the ball (small stun), between the landing and the get-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Landing", meta = (ClampMin = "0", Units = "Seconds"))
	float MissStunDuration = 0.25f;

	/**
	 * Play rate of the recovery after the touchdown: the landing (LandAnim) and the get-up (GetUpAnim) play this much faster, and their phases
	 * last their length divided by it (2 = half as long). ABP_SoccerPlayer reads it for the Play Rate of both sequence players.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Landing", meta = (ClampMin = "0.1"))
	float RecoveryPlayRate = 2.f;

	/** Seconds of the landing when LandAnim is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Landing", meta = (ClampMin = "0", Units = "Seconds"))
	float LandFallbackDuration = 0.25f;

	/** Seconds of the get-up when GetUpAnim is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Landing", meta = (ClampMin = "0", Units = "Seconds"))
	float GetUpFallbackDuration = 0.45f;

	// ---- Feedback ----

	/** Overlay material flashed once on the character mesh at takeoff (M_PlayerGraceFlash). Empty: no flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Feedback")
	TObjectPtr<UMaterialInterface> FlashMaterial;

	/** Seconds the takeoff flash stays on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Feedback", meta = (ClampMin = "0.02", Units = "Seconds"))
	float FlashDuration = 0.1f;

	// ---- Animation ----

	/** Takeoff, played once at the start of the jump (AS_Dive_Jump). The Takeoff phase lasts its length (at most AirDuration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Animation")
	TObjectPtr<UAnimSequenceBase> JumpAnim;

	/** Loop in the air, arms stretched (AS_Dive_Air). Played for the rest of AirDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Animation")
	TObjectPtr<UAnimSequenceBase> AirAnim;

	/** Landing on the floor, ends lying down (AS_Dive_Land). The Land phase lasts its length, plus MissStunDuration after a miss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Animation")
	TObjectPtr<UAnimSequenceBase> LandAnim;

	/** Get-up from the floor (AS_Dive_GetUp). The GetUp phase lasts its length, then the player is Free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Animation")
	TObjectPtr<UAnimSequenceBase> GetUpAnim;
};
