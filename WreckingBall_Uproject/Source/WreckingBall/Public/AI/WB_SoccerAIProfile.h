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

	// ---- Pass reception ----

	/**
	 * On: a bot a teammate passes the ball to takes the intent ReceivePass: it runs to the first point of the ball's path it reaches in time,
	 * faces the ball and walks into it. Ends when anyone takes the ball (an opponent may intercept) or after ReceivePassMaxTime.
	 * Off (default): the bot keeps its role while the pass travels.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Receive")
	bool bReceivePasses = false;

	/** Seconds after the pass during which the receiver keeps ReceivePass. After that, it plays its role again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Receive", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bReceivePasses"))
	float ReceivePassMaxTime = 3.f;

	/** Seconds of margin: the bot aims for the first point of the ball's path it reaches this much before the ball. Higher: it comes earlier and waits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Receive", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bReceivePasses"))
	float ReceivePassMargin = 0.2f;

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

	/**
	 * True: the Dash direction is the direction to the target's current position (plus DashAngleJitter), an approximate aim at the player.
	 * False: the bot aims at the predicted position (DashLeadFactor). The reach check always uses the predicted position.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash")
	bool bDashAimAtTargetLocation = false;

	/** Seconds between two Dashes of the bot on a player. Ready again at once after a goal (the match reset calls ResetDashCooldown). The wall breach uses BreachDashCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", Units = "Seconds"))
	float DashCooldown = 2.5f;

	/** Seconds between two Dashes of the bot into a wall it breaks through (bAllowBreach). Separate from DashCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", Units = "Seconds"))
	float BreachDashCooldown = 2.5f;

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
	 * Defensive zone of the Defender: a radius measured from the net perimeter of its goal (UWB_GoalkeeperZoneComponent), equal to half the
	 * distance between the two goals times this. 1: half of the field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0.1", ClampMax = "2"))
	float DefendRadiusScale = 1.f;

	/** The Defender stands on the line ball - own goal, this many cm from the ball (or from the carrier), toward its goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0"))
	float DefendGapToBall = 400.f;

	/**
	 * True: while the team defends, the Defender keeps about DefenderTeammateDistance from its teammate Attacker (else from the teammate who presses).
	 * Its defense spot is moved toward or away from that teammate, in the same direction. Never applies to the Goalkeeper.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender")
	bool bDefenderKeepsTeammateDistance = false;

	/** Distance (cm) the Defender keeps from its teammate Attacker while the team defends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0", Units = "cm", EditCondition = "bDefenderKeepsTeammateDistance"))
	float DefenderTeammateDistance = 800.f;

	/** Plus or minus margin (cm) around DefenderTeammateDistance: inside it, the defense spot is not moved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender", meta = (ClampMin = "0", Units = "cm", EditCondition = "bDefenderKeepsTeammateDistance"))
	float DefenderTeammateTolerance = 150.f;

	// ---- Defender, the team holds the ball: open spot ("blind spot") near the carrier ----

	/** The Defender looks for an open spot on a circle of this radius (cm) around the carrier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "100"))
	float SupportOpenDistance = 600.f;

	/** Number of spots tested on that circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "4", ClampMax = "32"))
	int32 SupportOpenSampleCount = 12;

	/** Seconds between two searches of the open spot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0.1"))
	float SupportOpenInterval = 0.5f;

	/**
	 * Score of a spot (cm): distance to the nearest opponent + LaneWeight x distance of the nearest opponent to the pass lane (carrier - spot)
	 * + ForwardBias x how much the spot is toward the opponent goal (-1..1). Higher is better.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0"))
	float SupportOpenLaneWeight = 1.f;

	/** See SupportOpenLaneWeight. Higher: the Defender prefers the spots toward the opponent goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support")
	float SupportOpenForwardBias = 200.f;

	/** Hysteresis: the Defender changes spot only when a new one scores this many cm better. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0"))
	float SupportOpenSwitchGain = 150.f;

	/** Every SupportAheadRollInterval seconds, chance (0..1) that the Defender runs ahead of the ball instead (SupportLeadDistance), when that spot is in shooting range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0", ClampMax = "1"))
	float SupportAheadChance = 0.25f;

	/** See SupportAheadChance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0.5"))
	float SupportAheadRollInterval = 3.f;

	/** Seconds the Defender stays ahead of the ball once the roll succeeded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Defender|Support", meta = (ClampMin = "0"))
	float SupportAheadDuration = 2.5f;

	// ---- Goalkeeper ----

	/**
	 * The Goalkeeper stands this many cm out of the net perimeter (its UWB_GoalkeeperZoneComponent box), on the line ball - goal center.
	 * Without a zone, measured from the goal aim point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float GoalkeeperDistance = 300.f;

	/** When the threat (opponent carrier, else the ball) comes within this many cm of the net perimeter, the Goalkeeper falls back inside its zone. 0: never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float GoalkeeperRetreatDistance = 600.f;

	/** Fallen back, the Goalkeeper stands this many cm inside the net perimeter, on the line threat - goal center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float GoalkeeperInsideDepth = 80.f;

	/** True: a Goalkeeper bot enters the defense stance (Player.State.Defending) while it stands inside its zone, and leaves it once out of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper")
	bool bGoalkeeperAutoDefense = true;

	/** Hysteresis of the defense stance: the Goalkeeper leaves it only once this many cm out of its zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0"))
	float GoalkeeperZoneExitMargin = 50.f;

	/** True: a free ball inside its zone makes the Goalkeeper go for it (in the stance, the touch catches it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper")
	bool bGoalkeeperClaimsBallInZone = true;

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

	// ---- Breach: break walls instead of a long detour (user decision 2026-11-20) ----

	/**
	 * True: when a bot moves somewhere, it compares the NavMesh path (through the doors) with the straight line through the destructible walls
	 * (Geometry Collections, locked doors). Straight line time = distance / walk speed + BreachWallCostSeconds per wall; detour time = path length / walk speed
	 * (none when the NavMesh finds no complete path). The faster one wins: the bot Dashes into the first wall of the straight line.
	 * A wall that cannot break (no Geometry Collection) on the line rules the straight line out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach")
	bool bAllowBreach = false;

	/** Estimated seconds to break one wall (run-up, Dashes, energy). Higher: the bots take the detour more often. 0: always the straight line when it is breakable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach", meta = (ClampMin = "0"))
	float BreachWallCostSeconds = 3.f;

	/** Height (cm) above the bot location of the straight line trace that finds the walls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach")
	float BreachTraceHeight = 30.f;

	/** At most this many walls counted on the straight line; more: the straight line is ruled out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach", meta = (ClampMin = "1", ClampMax = "10"))
	int32 BreachMaxWalls = 4;

	/** A bot that has been breaking the same wall this many seconds gives up on it (it is then ignored for BreachIgnoreDuration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach", meta = (ClampMin = "1"))
	float BreachMaxDuration = 12.f;

	/** Seconds a wall the bot gave up on is ignored by the breach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Breach", meta = (ClampMin = "0"))
	float BreachIgnoreDuration = 20.f;

	// ---- Debug ----

	/** Draws the path, the intent and the shooting zone of each bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug")
	bool bDrawDebug = false;

	// ---- Debug filter: switch bot intents and actions off one by one, for debugging (user decision 2026-11-20) ----

	/**
	 * True: the switches below apply (an intent or an action switched off is never used by the bots). False: every intent and action is allowed,
	 * the bots play normally. Can be changed during Play In Editor. Other switches already exist: bUseTeamRoles, bAllowBreach,
	 * bGoalkeeperAutoDefense (defense stance), bGoalkeeperClaimsBallInZone, bPressWhileAttacking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter")
	bool bUseFeatureFilter = false;

	/** Intent SeekBall (run into the free ball). Off: the bot stays Idle instead (a Goalkeeper claiming a ball in its zone keeps GuardGoal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowSeekBall = true;

	/** Intent ChaseCarrier (chase the opponent carrier and Dash on it). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowChaseCarrier = true;

	/** Intent Support (run ahead of the teammate carrier, legacy brain and Attacker role). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowSupport = true;

	/** Intent Attack (the bot holds the ball: reach the shooting zone and shoot). Off: the bot stays Idle with the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowAttack = true;

	/** Intent Press (team roles: press the opponent attacker given by the coordinator). Off: the pressing bot keeps the intent of its role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowPress = true;

	/** Intent Defend (team roles, Defender while the team defends). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowDefend = true;

	/** Intent GuardGoal (team roles, Goalkeeper in front of its goal). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowGuardGoal = true;

	/** Intent LastMan (team roles, Goalkeeper moving up while the team holds the ball). Off: the Goalkeeper keeps GuardGoal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowLastMan = true;

	/** Intent SupportOpen (team roles, Defender in an open spot near the carrier). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowSupportOpen = true;

	/** Intent ReceivePass (run to the path of a pass meant for the bot, when bReceivePasses is on). Off: the receiver keeps the intent of its role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowReceivePass = true;

	/** Action Dash (on a player, and the wall breach). Off: the bot never Dashes; ChaseCarrier and Press only follow the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Actions", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowDash = true;

	/** Action shoot. Off: in Attack the bot reaches the shooting zone, aims at the goal and waits, without kicking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Actions", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowShoot = true;

	/**
	 * Action ball pickup. Off: in SeekBall the bot stops GrabRange away from the ball and faces it, instead of walking into it.
	 * The catch itself stays automatic: a ball rolling into the bot is still caught.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Actions", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowBallPickup = true;
};
