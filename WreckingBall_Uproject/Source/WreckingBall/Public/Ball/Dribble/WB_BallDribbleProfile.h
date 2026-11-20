#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_BallDribbleProfile.generated.h"

class USoundBase;

/**
 * How a possessed ball is dribbled on the ground (DA_BallDribble_Default, dribble devis 2026-11-19).
 * Read every frame by UWB_BallDribbleComponent: the ball stays fully simulated (gravity, floor, bounces) and is only pushed flat
 * toward a point in front of the feet, while it rolls. A ball's PDA_PhysicsProfile can point to its own profile (DribbleProfile).
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_BallDribbleProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Dribble point ----

	/** Gap in cm between the capsule and the ball. The dribble point is at capsule radius + ball radius + this, in front of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Point", meta = (Units = "Centimeters"))
	float FrontGap = 5.f;

	/** The dribble point moves ahead by the player's flat velocity x this, in seconds, so the ball stays in front while running. 0: no lead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Point", meta = (ClampMin = "0", Units = "Seconds"))
	float LeadTime = 0.08f;

	// ---- Follow (flat only) ----

	/** How fast the position gap to the dribble point closes, per second: the target speed of the ball is the player's speed + gap x this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Follow", meta = (ClampMin = "0"))
	float FollowStiffness = 8.f;

	/** How fast the ball takes its target speed, per second: higher feels tighter, lower feels looser. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Follow", meta = (ClampMin = "0"))
	float FollowResponse = 12.f;

	/** Cap of the flat push on the ball, in cm/s2. Lower lets furniture block the ball and opponents push it away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Follow", meta = (ClampMin = "0"))
	float MaxAcceleration = 6000.f;

	/** Extra gap in cm kept around the capsule: on a sharp turn, the ball goes around the player instead of through it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Follow", meta = (ClampMin = "0", Units = "Centimeters"))
	float CapsuleClearance = 10.f;

	// ---- Roll ----

	/** How fast the ball spin follows the ideal roll (flat speed / radius), per second. 0: the spin is left to the physics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Roll", meta = (ClampMin = "0"))
	float RollFollowSpeed = 15.f;

	/** The roll is driven only while the bottom of the ball is within this many cm of the floor. Higher in the air, the spin stays free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Roll", meta = (ClampMin = "0", Units = "Centimeters"))
	float GroundTolerance = 5.f;

	// ---- Elastic tether (test) ----

	/** On: beyond TetherDistance, an extra pull brings the ball back toward the dribble point. Off (default): no tether at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Tether")
	bool bUseTether = false;

	/** Flat distance in cm from the dribble point beyond which the tether pulls. Keep it below LoseDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Tether", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUseTether"))
	float TetherDistance = 40.f;

	/** Tether pull in cm/s2 per cm beyond TetherDistance: the farther the ball, the harder it comes back. 0: no tether. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Tether", meta = (ClampMin = "0", EditCondition = "bUseTether"))
	float TetherStiffness = 60.f;

	/** Cap of the tether pull, in cm/s2, so an opponent can still tear the ball away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Tether", meta = (ClampMin = "0", EditCondition = "bUseTether"))
	float TetherMaxAcceleration = 4000.f;

	// ---- Loss ----

	/** Flat distance in cm from the dribble point beyond which the possession is lost (blocked by furniture, pushed by an opponent...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Loss", meta = (ClampMin = "0", Units = "Centimeters"))
	float LoseDistance = 90.f;

	/** Seconds the ball must stay beyond LoseDistance before the possession is lost, so a short bump is forgiven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Loss", meta = (ClampMin = "0", Units = "Seconds"))
	float LoseDelay = 0.25f;

	/** Seconds after a catch during which LoseHeight is ignored, so a bouncing ball caught in the air settles at the feet instead of being lost at once. 0: LoseHeight applies at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Loss", meta = (ClampMin = "0", Units = "Seconds"))
	float CatchHeightGrace = 0.f;

	/** Height in cm of the bottom of the ball above the player's feet beyond which the possession is lost at once (bounce, fall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Loss", meta = (ClampMin = "0", Units = "Centimeters"))
	float LoseHeight = 80.f;

	// ---- Ball touches (option) ----

	/**
	 * On: the ball is dribbled by touches. When the player catches up with it, a touch sends it ahead; between touches only its side drift is corrected.
	 * Off (default): the ball follows the dribble point continuously (tight control).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Touches")
	bool bUseTouches = false;

	/** Distance in cm ahead of the dribble point a touch sends the ball to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Touches", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUseTouches"))
	float TouchLeadDistance = 60.f;

	/** Flat speed of the ball after a touch = player's speed x this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Touches", meta = (ClampMin = "0", EditCondition = "bUseTouches"))
	float TouchSpeedFactor = 1.4f;

	/** Minimum seconds between two touches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Touches", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bUseTouches"))
	float TouchMinInterval = 0.3f;

	/** Under this player speed (cm/s), touches stop and the ball follows the dribble point continuously, so it settles at the feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Touches", meta = (ClampMin = "0", EditCondition = "bUseTouches"))
	float TouchMinPlayerSpeed = 150.f;

	// ---- Feedback ----

	/** Sound played at the ball each time a player takes possession of it (catch, pass received, steal). Empty: no sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Feedback")
	TObjectPtr<USoundBase> CatchSound;

	/** Volume multiplier of CatchSound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble|Feedback", meta = (ClampMin = "0"))
	float CatchSoundVolume = 1.f;
};
