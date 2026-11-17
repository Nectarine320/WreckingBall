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
	// ---- Trigger ----

	/** Ball actors the dive looks for (BP_PO_Master_Ball_01 and its children). Empty: the dive never starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Trigger")
	TSubclassOf<AActor> BallClass;

	/** Minimum ball speed in cm/s for the ball to count as kicked and in flight. Slower (rolling to a stop, held): no dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Trigger", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MinBallSpeed = 400.f;

	/** Balls farther than this (cm, from the player) are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Trigger", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxBallDistance = 1500.f;

	/** True: only a ball last kicked or thrown by a player of the other team starts a dive (a ball with no team or no player counts as enemy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Trigger")
	bool bOnlyEnemyBall = true;

	// ---- Jump ----

	/** Seconds in the air, from takeoff to touchdown. The landing and the get-up come after. Main speed setting of the dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0.2", Units = "Seconds"))
	float AirDuration = 0.75f;

	/** Shortest dive in cm (flat distance from takeoff to touchdown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MinDistance = 200.f;

	/** Longest dive in cm. Also the distance of a dive that finds no reachable point on the ball path (straight along the stick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxDistance = 500.f;

	/** Lowest height in cm above the ground of the body at the top of the dive (a low dive along the floor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MinHeight = 50.f;

	/** Highest height in cm above the ground of the body at the top of the dive. The dive aims at the ball's height, clamped between MinHeight and this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Jump", meta = (ClampMin = "0", Units = "Centimeters"))
	float MaxHeight = 225.f;

	// ---- Aim ----

	/** Half angle in degrees of the cone around the stick direction where the dive looks for the ball path (30 = a 60 degree cone). Stick released: the whole circle. */
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

	/** Share of the ball speed kept when it bounces off the diver (0..1), away from the body like on a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Ball", meta = (ClampMin = "0", ClampMax = "1"))
	float BallBounceRestitution = 0.6f;

	// ---- Landing ----

	/** Seconds the player stays on the ground after a dive that missed the ball (small stun), between the landing and the get-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerDive|Landing", meta = (ClampMin = "0", Units = "Seconds"))
	float MissStunDuration = 0.5f;

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
