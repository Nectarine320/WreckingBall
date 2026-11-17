#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_SoccerAIProfile.generated.h"

/**
 * Tuning of a soccer bot (AWB_SoccerAIController_Base): navigation, ball pickup, Dash on the ball carrier, shooting.
 * Distances in cm, times in seconds, angles in degrees. Default asset: DA_SoccerAIProfile_Default.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_SoccerAIProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWB_SoccerAIProfile();

	// ---- World ----

	/** Class of the balls the bot looks for (children included). Default: BP_PO_Master_Ball_01. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|World")
	TSoftClassPtr<AActor> BallClass;

	/** Seconds between two searches of the nearest ball (balls respawn after a goal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|World", meta = (ClampMin = "0.1"))
	float BallSearchInterval = 1.f;

	// ---- Navigation ----

	/** Distance to the destination under which the bot stops moving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Navigation", meta = (ClampMin = "0"))
	float AcceptanceRadius = 60.f;

	/** Distance to a path point under which the bot heads to the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Navigation", meta = (ClampMin = "1"))
	float WaypointRadius = 80.f;

	/** Seconds between two path computations toward a moving target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Navigation", meta = (ClampMin = "0.05"))
	float RepathInterval = 0.3f;

	/** The path is recomputed at once when the destination moved more than this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Navigation", meta = (ClampMin = "0"))
	float RepathDistance = 150.f;

	/** Box half-size used to snap a destination (ball in the air, player) onto the NavMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Navigation")
	FVector NavProjectExtent = FVector(200.f, 200.f, 500.f);

	// ---- Ball ----

	/** Distance to the ball under which the bot faces it and sends Grab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Ball", meta = (ClampMin = "0"))
	float GrabRange = 130.f;

	/** Seconds between two Grab attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Ball", meta = (ClampMin = "0"))
	float GrabRetryInterval = 0.4f;

	// ---- Dash ----

	/** The bot Dashes on the opponent ball carrier when the distance is between DashMinDistance and DashMaxDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0"))
	float DashMinDistance = 300.f;

	/** See DashMinDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0"))
	float DashMaxDistance = 600.f;

	/** Random yaw (plus or minus) added to the Dash direction, so a Dash can miss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", ClampMax = "90"))
	float DashAngleJitter = 15.f;

	/** Seconds between two Dash attempts of the bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0"))
	float DashCooldown = 2.5f;

	// ---- Shoot ----

	/** The bot shoots when its distance to the aim point of the opponent goal is between ShootMinDistance and ShootMaxDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0"))
	float ShootMinDistance = 500.f;

	/** See ShootMinDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0"))
	float ShootMaxDistance = 1500.f;

	/** Distance from the goal of the spot the bot runs to when it is out of the shooting zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0"))
	float ShootPreferredDistance = 1000.f;

	/** Half-angle of the shooting zone, around the direction from the goal toward the field. Wider: shots from the sides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0", ClampMax = "90"))
	float ShootConeHalfAngle = 50.f;

	/** Random yaw (plus or minus) added to the aim at each shot. 0: always the goal center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0", ClampMax = "45"))
	float ShootAimJitter = 5.f;

	/** The charge starts once the bot faces the aim within this angle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0.5"))
	float ShootAimTolerance = 8.f;

	/** Turn speed of the bot while aiming (degrees per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "1"))
	float AimTurnSpeed = 540.f;

	/** Charge duration of a shot from ShootMinDistance. Interpolated up to ChargeTimeAtMaxDistance with the distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0"))
	float ChargeTimeAtMinDistance = 0.15f;

	/** Charge duration of a shot from ShootMaxDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0"))
	float ChargeTimeAtMaxDistance = 0.9f;

	// ---- Support ----

	/** When a teammate carries the ball, the bot runs this far ahead of the carrier, toward the opponent goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Support", meta = (ClampMin = "0"))
	float SupportLeadDistance = 400.f;

	/** Side offset of the support spot, so the bot does not stand in the carrier's way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Support", meta = (ClampMin = "0"))
	float SupportLateralOffset = 300.f;

	// ---- Debug ----

	/** Draws the path, the intent and the shooting zone of each bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug")
	bool bDrawDebug = false;
};
