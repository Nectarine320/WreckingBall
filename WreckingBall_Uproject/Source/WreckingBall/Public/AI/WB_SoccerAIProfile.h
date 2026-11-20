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

	/** Distance to the ball under which the bot stops following its path and walks straight into the ball (the catch is automatic). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Ball", meta = (ClampMin = "0"))
	float GrabRange = 130.f;

	// ---- Dash ----

	/**
	 * Fraction of the real Dash reach used by the bot. Reach: the Dash distance of the pawn (DashScanDistance) plus both capsule radii.
	 * The bot Dashes only when the predicted position of the target is within Reach x DashReachRatio. Lower: safer but rarer Dashes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float DashReachRatio = 0.9f;

	/** Extra cm added to the minimum run-up of the pawn (DashMinRunUpDistancePlayer): closer than that, the Dash would not knock Down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0"))
	float DashRunUpMargin = 20.f;

	/** Energy (0..1) the bot keeps after a Dash: it Dashes only when Energy >= Dash cost + this reserve. 0: may empty the bar (then exhausted). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", ClampMax = "1"))
	float DashEnergyReserve = 0.1f;

	/** How much the bot leads the target: aims at Location + Velocity x Dash travel time x this. 0: aims at the current position. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", ClampMax = "2"))
	float DashLeadFactor = 1.f;

	/** True: no Dash when a wall or a non-physics object stands on the Dash line (the Dash would stop or bounce back). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash")
	bool bDashNeedsClearLine = true;

	/** True: no Dash when a teammate the Dash would knock Down stands on the line (friendly fire). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash")
	bool bDashAvoidTeammates = true;

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

	/** Out of the shooting zone, the bot runs to a spot inside this share of ShootConeHalfAngle (0.8: a bit inside the edge, so it does not stop right on the border). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Shoot", meta = (ClampMin = "0", ClampMax = "1"))
	float ShootSpotConeScale = 0.8f;

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

	// ---- Team (phase F: coordinated roles) ----

	/**
	 * True: the bots play the role given by the team coordinator (UWB_TeamCoordinatorComponent on the GameMode): Attacker, Defender, Goalkeeper.
	 * False, or no coordinator: every bot uses the legacy brain (everyone goes for the ball).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team")
	bool bUseTeamRoles = false;

	/** Seconds between two role evaluations of a team coordinator. A possession change or a character switch also triggers one at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team", meta = (ClampMin = "0.05"))
	float RoleUpdateInterval = 0.3f;

	/**
	 * Hysteresis against role flicker: a member takes the Attacker (or Goalkeeper) role from the current one only when it is nearer to the ball
	 * (or to its goal) by more than this many cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team", meta = (ClampMin = "0"))
	float RoleSwitchMargin = 150.f;

	/** Hysteresis: a member keeps a new role at least this many seconds (a possession change bypasses it: the carrier is the Attacker at once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team", meta = (ClampMin = "0"))
	float RoleMinHoldTime = 0.75f;

	/**
	 * True: the press rule (one bot always presses the opponent attacker) also applies while the team holds the ball.
	 * False: only while the team defends (free ball or opponent ball).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team")
	bool bPressWhileAttacking = false;

	/** A pressing bot whose target does not hold the ball marks it: it stands this many cm from the target, on the side of the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team", meta = (ClampMin = "0"))
	float PressMarkDistance = 150.f;

	/** Draws the role of every team member (human or bot) above its head, and the press link. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Team")
	bool bDrawRoleDebug = false;

	// ---- Defender ----

	/**
	 * Defensive zone of the Defender: a radius around its own goal, equal to half the distance between the two goals times this.
	 * 1: half of the field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0.1", ClampMax = "2"))
	float DefendRadiusScale = 1.f;

	/** The Defender stands on the line ball - own goal, this many cm from the ball (or from the carrier), toward its goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0"))
	float DefendGapToBall = 400.f;

	// ---- Goalkeeper ----

	/** The Goalkeeper stands this many cm in front of its goal, on the line ball - goal center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float GoalkeeperDistance = 300.f;

	/**
	 * When the team holds the ball, the Goalkeeper stays in its goal while the ball is in its own territory, and moves up as the last man (a defender)
	 * once the ball goes past this share of the way from its goal to the opponent goal. 0.5: the middle line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0", ClampMax = "1"))
	float GoalkeeperAdvanceFieldShare = 0.5f;

	/**
	 * Last man (the team holds the ball): the Goalkeeper moves up to this share of the way from its goal to the ball,
	 * and stays the member of its team nearest to its goal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0", ClampMax = "1"))
	float LastManBallShare = 0.35f;

	/** Last man: cm kept between the Goalkeeper and its goal, compared with the teammate nearest to that goal (it stays behind that teammate). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float LastManBehindMargin = 200.f;

	// ---- Debug ----

	/** Draws the path, the intent and the shooting zone of each bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug")
	bool bDrawDebug = false;
};
