#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/Team/WB_SoccerTeamTypes.h"
#include "WB_SoccerAIProfile.generated.h"

/** Sight of a bot (AIPerception, sense Sight) for one team role. */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_BotSightSettings
{
	GENERATED_BODY()

	/** Distance in cm at which the bot starts to see something. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", Units = "Centimeters"))
	float SightRadius = 2500.f;

	/** Distance in cm at which something already seen is lost (keep it above SightRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", Units = "Centimeters"))
	float LoseSightRadius = 2800.f;

	/** Half angle in degrees of the view cone, around the facing of the bot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float PeripheralHalfAngle = 75.f;
};

/**
 * Tuning of a soccer bot (AWB_SoccerAIController_Base): navigation, ball pickup, Dash on the ball carrier, shooting.
 * Distances in cm, times in seconds, angles in degrees. Default asset: DA_SoccerAIProfile_Default.
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.AI"))
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

	/** Charge (0..1) of the bot's Dash on a player (charged Dash, DA_PlayerDash_Default). The bot Dashes at once, without holding. 1: full Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", ClampMax = "1"))
	float BotDashCharge = 1.f;

	/** Charge (0..1) of the bot's Dash into a wall it breaks through (bAllowBreach). 1: full Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash", meta = (ClampMin = "0", ClampMax = "1"))
	float BotBreachDashCharge = 1.f;

	/**
	 * Rubber band. On: the bots of the team that trails in score Dash on players more often: their cooldown is RubberBandDashCooldowns
	 * for the goal gap instead of DashCooldown. Tied score and leading team: DashCooldown. Off (default): always DashCooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash|Rubber Band")
	bool bUseRubberBandDash = false;

	/**
	 * Rubber band: Dash cooldown in seconds of a trailing team's bot, by goal gap. Entry 0: 1 goal behind, entry 1: 2 goals behind, and so on;
	 * a larger gap uses the last entry. Empty: DashCooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash|Rubber Band", meta = (EditCondition = "bUseRubberBandDash"))
	TArray<float> RubberBandDashCooldowns = { 5.f, 4.f };

	/** Rubber band: number variable of the game mode holding the score of team 0 (red). BP_SoccerGamemode: ScoreRed. Technical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash|Rubber Band", meta = (EditCondition = "bUseRubberBandDash"))
	FName ScoreTeam0Variable = TEXT("ScoreRed");

	/** Rubber band: number variable of the game mode holding the score of team 1 (blue). BP_SoccerGamemode: ScoreBlue. Technical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Dash|Rubber Band", meta = (EditCondition = "bUseRubberBandDash"))
	FName ScoreTeam1Variable = TEXT("ScoreBlue");

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

	// ---- Goalkeeper (devis gardien): behavior AI.Behavior.Goalkeeping ----

	/** Goalkeeper holding the ball (after a catch): charge time in seconds of its clearing kick toward the opponent goal. Longer = stronger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper", meta = (ClampMin = "0", Units = "Seconds"))
	float GoalkeeperClearChargeTime = 0.4f;

	/** Draws the Goalkeeper brain: threat, task, guard spot, shot path, dive, shield grid rays (green: blocked, red: open) and coverage, remembered objects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper")
	bool bDrawGoalkeeperDebug = false;

	// ---- Goalkeeper, Threat ----

	/**
	 * Threat condition, checked every frame inside the team zone (UWB_TeamZoneComponent of the team): an opponent holds the ball; or an opponent is there
	 * and the ball too (free or not); or a teammate holds the ball and an opponent is within this many cm of it. A threat stops the No threat branch at once
	 * (the held object is dropped). Without a team zone in the level: always a threat (a warning is logged).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float ThreatNearAllyCarrierDistance = 300.f;

	/** P1: the Goalkeeper out of its zone runs back (sprint) while farther than this many cm from its goal, and walks (default speed) once closer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float GoalkeeperRunDistance = 500.f;

	/** P2: the Goalkeeper stands this many cm out of the net perimeter (its UWB_GoalkeeperZoneComponent box), on the line ball - goal center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float GoalkeeperDistance = 300.f;

	/** True: in its zone, the Goalkeeper takes the defense stance (Player.State.Defending, the only way to catch a kicked ball). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat")
	bool bGoalkeeperAutoDefense = true;

	/** Hysteresis of the zone: the Goalkeeper counts as out of its zone (P1, stance left) only once this many cm out of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float GoalkeeperZoneExitMargin = 50.f;

	/** True: a free ball inside its zone makes the Goalkeeper go for it (in the stance, the touch catches it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat")
	bool bGoalkeeperClaimsBallInZone = true;

	/** P2 look: a free ball is looked at while within this many cm of the net perimeter; farther, the Goalkeeper looks at the nearest opponent in the zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float GoalkeeperLookFreeBallDistance = 500.f;

	/** P2 look: seconds before the Goalkeeper turns toward the new carrier after a pass between opponents (first pass of a chain). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Seconds"))
	float PassLookDelayBase = 0.1f;

	/** P2 look: seconds added to the delay for each pass chained within PassChainWindow of the previous one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Seconds"))
	float PassLookDelayStep = 0.1f;

	/** P2 look: a pass made within this many seconds of the previous one is chained. No pass for that long: the delay is back to PassLookDelayBase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Seconds"))
	float PassChainWindow = 0.3f;

	/** P2 look: the pass delay never goes past this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Seconds"))
	float PassLookDelayMax = 0.5f;

	/** P2 difficulty: the guard spot follows where the ball (or carrier) was this many seconds ago. 0: instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Seconds"))
	float ReactionTime = 0.15f;

	/** P2 difficulty: 1 = always on the line ball - goal center; lower = a random side error up to (1 - this) x PositionErrorMax. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", ClampMax = "1"))
	float PositioningAccuracy = 0.8f;

	/** P2 difficulty: largest side error in cm of the guard spot (PositioningAccuracy 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0", Units = "Centimeters"))
	float PositionErrorMax = 120.f;

	/** P2 difficulty: seconds between two draws of the side error. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Threat", meta = (ClampMin = "0.1", Units = "Seconds"))
	float PositionErrorInterval = 1.5f;

	/** P3: a kicked ball counts as a shot when its path crosses the goal mouth (MouthHalfWidth and MouthHeight of the goal) grown by this many cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", Units = "Centimeters"))
	float ShotMouthMargin = 50.f;

	/** P3: the shot path passes within this many cm of the Goalkeeper: it is already on it and slides in the stance to stay on it (no dive). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", Units = "Centimeters"))
	float ShotOnBodyDistance = 70.f;

	/** P3: the Goalkeeper dives only while within this many cm of its post (its UWB_GoalkeeperZoneComponent box grown by GoalkeeperDistance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", Units = "Centimeters"))
	float DiveMaxDistance = 300.f;

	/**
	 * P3: side angle of the dive in degrees, measured from the shot line: 90 = a pure side dive (square to the shot), lower = the dive also goes forward,
	 * toward the shooter (0 = straight at the ball). The dive then searches the ball in its own cone (DA_PlayerDive_Default).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float DiveAngle = 25.f;

	/** P3 difficulty: seconds between the shot detection and the dive (the slide starts at once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", Units = "Seconds"))
	float DiveReactionTime = 0.15f;

	/** P3 difficulty: 1 = the dive goes exactly toward the shot; lower = a random yaw error up to (1 - this) x DiveErrorMax. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", ClampMax = "1"))
	float DiveAccuracy = 0.8f;

	/** P3 difficulty: largest yaw error in degrees of the dive (DiveAccuracy 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shot", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float DiveErrorMax = 20.f;

	/** Shield grid: rays across the goal mouth (MouthHalfWidth and MouthHeight of the goal), columns along its width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ShieldGridColumns = 5;

	/** Shield grid: rows along the height of the goal mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ShieldGridRows = 3;

	/** Shield grid: length in cm of each ray, from the goal mouth toward the field. Only shield objects (Medium or bigger) block a ray. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "10", Units = "Centimeters"))
	float ShieldTraceLength = 350.f;

	/**
	 * The shield is certified when this share (0..1) of the rays is blocked, by one Large / XLarge object or by two Medium objects.
	 * Certified: the Goalkeeper leaves its goal and supports its team.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", ClampMax = "1"))
	float ShieldCoverageThreshold = 0.4f;

	/** Patrol: seconds the Goalkeeper looks around for shield objects (line of sight). A Large / XLarge seen is fetched at once; at the end, the nearest Medium. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Seconds"))
	float PatrolDuration = 7.f;

	/** Patrol: the Goalkeeper sweeps its look this many degrees to each side of the field direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float PatrolScanHalfAngle = 80.f;

	/** Patrol: turn speed of the look sweep, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1"))
	float PatrolScanSpeed = 60.f;

	/** Fetch: the Goalkeeper presses Grab once the object is within this many cm of its capsule (closest point of the object bounds), facing it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Centimeters"))
	float ShieldGrabReach = 60.f;

	/** Fetch and carry: seconds before the Goalkeeper gives up on an object it cannot reach or bring (the object is then skipped). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1", Units = "Seconds"))
	float ShieldTaskTimeout = 12.f;

	/** Placement: target point this many cm in front of the goalkeeper box (net perimeter), on the line ball - goal center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Centimeters"))
	float ShieldDropOffset = 100.f;

	/** Placement of two Medium objects: each one is this many cm to one side of the target point, along the goal mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Centimeters"))
	float ShieldMediumSpacing = 80.f;

	/** Placement: the object is let go once its predicted landing point (sweep down with its shape) is within this many cm of the target point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1", Units = "Centimeters"))
	float ShieldDropTolerance = 50.f;

	/**
	 * Placement, target out of reach (no full NavMesh path, or carried for half of ShieldTaskTimeout): the Goalkeeper throws the object at the smallest
	 * charge that reaches: charge = distance / this many cm (a full charge throws about this far). Its path is drawn with PredictProjectilePath after the release.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "100", Units = "Centimeters"))
	float ShieldThrowFullChargeDistance = 1200.f;

	/** Placement: seconds the object is left to settle after the drop or the throw, before the coverage is measured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Seconds"))
	float ShieldSettleTime = 1.f;

	/** Placement: tries on the same object (fetch it again and replace it) while the coverage stays too low. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1"))
	int32 ShieldMaxAttempts = 3;

	/** Shield moved: a shield object seen this many cm from where it was placed counts as moved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "1", Units = "Centimeters"))
	float ShieldMovedDistance = 100.f;

	/** Shield moved: seconds the place of a shield object must stay in view without the object before it counts as gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Shield", meta = (ClampMin = "0", Units = "Seconds"))
	float ShieldMissingConfirmTime = 0.5f;

	/** Supporter (no shield found): the Goalkeeper stays at least this many cm from its goal, between the ball and the goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Supporter", meta = (ClampMin = "0", Units = "Centimeters"))
	float SupporterMinDistance = 800.f;

	/** Supporter: the Goalkeeper stays at most this many cm from its goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Supporter", meta = (ClampMin = "0", Units = "Centimeters"))
	float SupporterMaxDistance = 1200.f;

	/** Supporter: cm kept between the Goalkeeper and the ball, on the goal side (it never stands past the ball). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Supporter", meta = (ClampMin = "0", Units = "Centimeters"))
	float SupporterGapToBall = 300.f;

	/** Supporter: every this many seconds, the Goalkeeper checks its goal again and patrols when it is still unprotected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Goalkeeper|Supporter", meta = (ClampMin = "1", Units = "Seconds"))
	float SupporterRepatrolInterval = 15.f;

	// ---- Sight (all bots) ----

	/**
	 * Sight of the bots (AIPerception, sense Sight), by team role. Walls and objects block the view. A role missing from the map uses DefaultSight.
	 * The Goalkeeper finds its shield objects with it (only actors with an AIPerceptionStimuliSource, or pawns, are seen).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight")
	TMap<EWB_SoccerRole, FWB_BotSightSettings> RoleSight;

	/** Sight of a role missing from RoleSight (and of the bots without a role). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight")
	FWB_BotSightSettings DefaultSight;

	/** Seconds a seen object stays in the memory of a bot after it was last seen (its last seen place is kept). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight", meta = (ClampMin = "0", Units = "Seconds"))
	float ObjectMemoryDuration = 60.f;

	/** Seen objects: names of the getter (Blueprint interface of the impact sources) and of the Gameplay Tag variable of its PDA_PhysicsProfile. Technical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight")
	FName PhysicsProfileFunction = TEXT("GetPhysicsProfile");

	/** See PhysicsProfileFunction. Technical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight")
	FName SizeTagVariable = TEXT("SizeTag");

	/** Actor tag of the objects nobody may grab (BPC_HeldObjectActions refuses them). Checked before the size: such an object is ignored. Technical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Sight")
	FName NoGrabTag = TEXT("NoGrab");

	// ---- Forced roles (gyms) ----

	/** Forced role Aggressive (AI.ForcedRole.Aggressive), holding the ball: distance in cm it keeps from the nearest opponent (human or bot). It moves away while the opponent is nearer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Forced Roles", meta = (ClampMin = "0", Units = "Centimeters"))
	float AggressiveKeepAwayDistance = 300.f;

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

	/** Intents GuardGoal and KeeperReturn (Goalkeeper, Threat branch P1 to P3: reach the zone, placement, look, slide). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowGuardGoal = true;

	/** Goalkeeper, No threat branch (patrol, fetch and place the shield objects, supporter). Off: with no threat, the Goalkeeper stays Idle in its goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowGoalkeeperShield = true;

	/** Intent SupportOpen (team roles, Defender in an open spot near the carrier). Off: the bot stays Idle instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowSupportOpen = true;

	/** Intent ReceivePass (run to the path of a pass meant for the bot, when bReceivePasses is on). Off: the receiver keeps the intent of its role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Intents", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowReceivePass = true;

	/** Action dive (Goalkeeper P3). Off: the Goalkeeper only slides in the stance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SoccerAI|Debug|Filter|Actions", meta = (EditCondition = "bUseFeatureFilter"))
	bool bAllowDive = true;

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
