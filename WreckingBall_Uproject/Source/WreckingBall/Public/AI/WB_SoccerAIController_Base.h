#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/Team/WB_SoccerTeamTypes.h"
#include "WB_SoccerAIController_Base.generated.h"

class ACharacter;
class UWB_PlayerCommandComponent;
class UWB_PlayerDiveComponent;
class UWB_GoalkeeperZoneComponent;
class UWB_PlayerHitComponent;
class UWB_PlayerStateComponent;
class UWB_SoccerAIProfile;
class UWB_TeamGoalComponent;

/** What a soccer bot is trying to do. Fixed list of the bot brain, chosen each frame by ChooseIntent. */
UENUM(BlueprintType)
enum class EWB_SoccerAIIntent : uint8
{
	/** Nothing to do (no ball, or the player state blocks actions). */
	Idle,
	/** The ball is free: run into it (the catch is automatic). */
	SeekBall,
	/** An opponent carries the ball: chase it and Dash on it when in range. */
	ChaseCarrier,
	/** A teammate carries the ball: run ahead of it, toward the opponent goal. */
	Support,
	/** The bot holds the ball: reach the shooting zone, aim, charge and shoot. */
	Attack,
	/** Team roles: press the opponent attacker given by the coordinator (chase and Dash when it holds the ball, else mark it on the ball side). */
	Press,
	/** Team roles, Defender while the team defends: stand between the ball (or carrier) and the own goal, inside the defensive zone. */
	Defend,
	/** Team roles, Goalkeeper while the team defends: stand in front of the own goal, on the line ball - goal center. */
	GuardGoal,
	/** Team roles, Goalkeeper while the team holds the ball: move up, staying the member of the team nearest to the own goal. */
	LastMan,
	/** Team roles, Defender while the team holds the ball: stand in an open spot near the carrier (free pass lane), sometimes ahead of the ball. */
	SupportOpen,
	/** A teammate passed the ball to this bot (AIProfile->bReceivePasses): run to the first point of the ball's path it reaches in time, face the ball and walk into it. */
	ReceivePass
};

/**
 * Soccer bot: possesses a BP_SoccerPlayer and plays like a human player, through its UWB_PlayerCommandComponent
 * (so the same states, energy and refusals apply). Its team is the TeamIndex of the pawn's UWB_PlayerHitComponent;
 * it scores in the goal (UWB_TeamGoalComponent) its team does not defend. Paths come from the NavMesh.
 * Brain: ChooseIntent picks an intent each frame (overridable in Blueprint), then a native routine runs it.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_SoccerAIController_Base : public AAIController
{
	GENERATED_BODY()

public:
	AWB_SoccerAIController_Base();

	/** Bot tuning (DA_SoccerAIProfile_Default). Required: empty, the bot stays idle and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerAI")
	TObjectPtr<UWB_SoccerAIProfile> AIProfile;

	/** Intent chosen this frame. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SoccerAI")
	EWB_SoccerAIIntent CurrentIntent = EWB_SoccerAIIntent::Idle;

	/** Intent refused this frame by the debug filter (AIProfile->bUseFeatureFilter); CurrentIntent is then its fallback. Idle when nothing was refused. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SoccerAI|Debug")
	EWB_SoccerAIIntent FilteredIntent = EWB_SoccerAIIntent::Idle;

	/** True when the debug filter of AIProfile allows Intent (always true when bUseFeatureFilter is off, and for Idle). */
	UFUNCTION(BlueprintPure, Category = "SoccerAI|Debug")
	bool IsIntentAllowed(EWB_SoccerAIIntent Intent) const;

	/** True: the bot keeps its pawn but does nothing (the pawn stands still). Set by the match roster (ToggleBots); false by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerAI")
	bool bBrainPaused = false;

	/**
	 * Seconds the bot waits after a Dash on a player: DashCooldown, or with bUseRubberBandDash and the bot's team trailing in score,
	 * RubberBandDashCooldowns for the goal gap (score read on the game mode, ScoreTeam0Variable / ScoreTeam1Variable).
	 */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	float GetDashCooldown() const;

	/** Makes the Dash on a player available at once (DashCooldown restarts at the next Dash). Called by the match roster after a goal (ResetCharactersToPlaces). */
	UFUNCTION(BlueprintCallable, Category = "SoccerAI")
	void ResetDashCooldown();

	/** Pauses or resumes the bot brain (bBrainPaused). A paused bot stops moving at once. */
	UFUNCTION(BlueprintCallable, Category = "SoccerAI")
	void SetBrainPaused(bool bPaused);

	/** Role given by the team coordinator (UWB_TeamCoordinatorComponent). None: legacy brain (no coordinator, or AIProfile->bUseTeamRoles off). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SoccerAI|Team")
	EWB_SoccerRole CurrentRole = EWB_SoccerRole::None;

	/** Situation of the team, given with the role. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SoccerAI|Team")
	EWB_TeamSituation TeamSituation = EWB_TeamSituation::Defending;

	/** Called by the team coordinator at each evaluation. InPressTarget: the opponent this bot must press (None when it does not press). */
	UFUNCTION(BlueprintCallable, Category = "SoccerAI|Team")
	void SetTeamRole(EWB_SoccerRole InRole, EWB_TeamSituation InSituation, APawn* InPressTarget);

	/** Opponent this bot presses (given by the coordinator). None when it does not press. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI|Team")
	APawn* GetPressTarget() const { return PressTarget.Get(); }

	/** Goal the bot's team defends. None when the level has no UWB_TeamGoalComponent for its team. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	UWB_TeamGoalComponent* GetGoalToDefend() const;

	/** Goalkeeper zone (net perimeter) of the goal the bot's team defends. None when that goal has no UWB_GoalkeeperZoneComponent. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	UWB_GoalkeeperZoneComponent* GetOwnGoalkeeperZone() const;

	/** Team of the possessed player (UWB_TeamLibrary::GetTeamOf: its PlayerState, else the copy in its UWB_PlayerHitComponent). -1: no team. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	int32 GetTeamIndex() const;

	/** Nearest ball found by the last search. Can be None. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	AActor* GetBall() const { return CachedBall.Get(); }

	/** Player currently holding a ball (Player.Holding.Ball), the bot itself included. None when the ball is free. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	APawn* FindBallCarrier() const;

	/** Goal the bot's team must score in. None when the level has no UWB_TeamGoalComponent. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	UWB_TeamGoalComponent* GetGoalToAttack() const;

	/** True when Other is a player of another team (or when either has no team). */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	bool IsOpponent(const APawn* Other) const;

	/** Wall the bot is breaking through (bAllowBreach). None when it follows the NavMesh. */
	UFUNCTION(BlueprintPure, Category = "SoccerAI|Breach")
	AActor* GetBreachWall() const { return BreachWall.Get(); }

	/** Why the bot did not Dash on the carrier at the last check (Ready when it could). Shown by bDrawDebug. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SoccerAI")
	FName DashBlockReason;

	/**
	 * True when a Dash on Target is favorable now: the bot has the energy and may Dash, Target can be knocked Down,
	 * the predicted position of Target is within the real Dash reach (and past the minimum run-up), and the line is clear
	 * (no wall, no teammate). AimPoint: predicted position to Dash toward (Target location when false). Sets DashBlockReason.
	 */
	UFUNCTION(BlueprintCallable, Category = "SoccerAI")
	bool EvaluateDash(APawn* Target, FVector& AimPoint);

	/** The bot profile (AIProfile). Null when it is not assigned (the bot then stays idle). */
	UFUNCTION(BlueprintPure, Category = "SoccerAI")
	const UWB_SoccerAIProfile* GetActiveProfile() const;

	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * Picks the intent of this frame. Attack when holding something. With a team role: Press when the coordinator gave a press target,
	 * Attacker = SeekBall / ChaseCarrier, Defender = Defend (Support when the team has the ball), Goalkeeper = GuardGoal (LastMan when the team has the ball).
	 * Without a role (legacy): ChaseCarrier / Support when another player has the ball, else SeekBall.
	 * Debug filter (AIProfile->bUseFeatureFilter): Press, the Goalkeeper claim and LastMan are skipped when switched off; Tick turns any other intent switched off into Idle.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SoccerAI")
	EWB_SoccerAIIntent ChooseIntent();

	/** Called when CurrentIntent changes. Hook for feedback or debug. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SoccerAI")
	void OnIntentChanged(EWB_SoccerAIIntent OldIntent, EWB_SoccerAIIntent NewIntent);

private:
	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerStateComponent> PlayerStateComp;

	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerHitComponent> PlayerHit;

	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerCommandComponent> Commands;

	/** Defense stance of the Goalkeeper. Optional: without it, no automatic stance. */
	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerDiveComponent> DiveComp;

	/** Open spot of SupportOpen: its angle index on the circle around the carrier (-1: none yet), and the next search time. */
	int32 SupportSpotIndex = -1;
	double NextSupportSpotTime = 0.0;

	/** SupportOpen: next roll for the "ahead of the ball" run, and the end of the current run. */
	double NextSupportAheadRollTime = 0.0;
	double SupportAheadUntil = 0.0;

	TWeakObjectPtr<AActor> CachedBall;
	double NextBallSearchTime = 0.0;

	/** Result of FindBallCarrier, computed once per frame (GFrameCounter of CarrierFrame): it is asked several times per Tick. */
	mutable TWeakObjectPtr<APawn> CachedCarrier;
	mutable uint64 CarrierFrame = MAX_uint64;

	/** Current path (world points) toward PathTarget, and the index of the point being followed. */
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	FVector PathTarget = FVector::ZeroVector;
	double NextRepathTime = 0.0;

	/** Earliest time of the next Dash on a player (DashCooldown). */
	double NextDashTime = 0.0;

	/** Earliest time of the next breach Dash into a wall (BreachDashCooldown). */
	double NextBreachDashTime = 0.0;

	/** Predicted target position of the last EvaluateDash (debug). */
	FVector LastDashAimPoint = FVector::ZeroVector;

	/** True when the capsule sweep from the bot toward AimPoint (Dash length) meets a wall or a teammate. Sets DashBlockReason. */
	bool IsDashLineBlocked(const ACharacter* Me, const APawn* Target, const FVector& AimPoint, float Length);

	/** Shot in progress: the bot pressed the kick and releases it after ChargeTarget seconds. */
	bool bCharging = false;
	float ChargeElapsed = 0.f;
	float ChargeTarget = 0.f;

	/** Aim offset (yaw) drawn once per shot. */
	bool bAimJitterDrawn = false;

	/** Missing AIProfile already reported. */
	bool bWarnedNoProfile = false;
	float AimJitterYaw = 0.f;

	/** Opponent to press, from the coordinator. */
	TWeakObjectPtr<APawn> PressTarget;

	/** Idle: stands still and faces the ball (when there is one). */
	void TickIdle(ACharacter* Me, float DeltaSeconds);
	void TickSeekBall(ACharacter* Me, float DeltaSeconds);
	void TickChaseCarrier(ACharacter* Me, float DeltaSeconds);
	void TickSupport(ACharacter* Me, float DeltaSeconds);
	void TickPress(ACharacter* Me, float DeltaSeconds);
	void TickDefend(ACharacter* Me, float DeltaSeconds);

	/** Defender (bDefenderKeepsTeammateDistance): moves Spot toward or away from the teammate Attacker (else the presser) to about DefenderTeammateDistance. */
	FVector KeepTeammateDistance(const ACharacter* Me, const FVector& Spot) const;
	void TickGuardGoal(ACharacter* Me, float DeltaSeconds);
	void TickLastMan(ACharacter* Me, float DeltaSeconds);
	void TickSupportOpen(ACharacter* Me, float DeltaSeconds);

	/** Ball passed to this bot (bReceivePasses), refreshed by ChooseIntent. */
	TWeakObjectPtr<AActor> IncomingPassBall;

	/** ReceivePass: point of the ball's path the bot runs to (debug). */
	FVector PassInterceptPoint = FVector::ZeroVector;

	/** True when a pass to this bot is travelling and younger than ReceivePassMaxTime. Sets IncomingPassBall. */
	bool RefreshIncomingPass();

	/** ReceivePass: runs to the ball's path, faces the ball, then walks into it. */
	void TickReceivePass(ACharacter* Me, float DeltaSeconds);

	/**
	 * First point of the ball's path (ball at BallLocation, flat velocity BallVelocity, slowing down by the pass profile's PassRollingDeceleration)
	 * the bot reaches at least ReceivePassMargin seconds before the ball. The end of the path when it reaches none.
	 */
	FVector GetPassInterceptPoint(const ACharacter* Me, const FVector& BallLocation, const FVector& BallVelocity) const;

	/** Score (cm) of an open spot for a pass from Carrier: see SupportOpenLaneWeight in the profile. OpponentLocations: from GatherOpponentLocations. */
	float ScoreSupportSpot(const FVector& CarrierLocation, const FVector& Spot, const FVector& Forward, const TArray<FVector>& OpponentLocations) const;

	/** Locations of the opponent players (pawns with a UWB_PlayerStateComponent), gathered once before scoring the open spots. */
	void GatherOpponentLocations(TArray<FVector>& OutLocations) const;

	/** World point of open spot Index on the circle around CarrierLocation (snapped on the NavMesh when possible). */
	FVector GetSupportSpot(const FVector& CarrierLocation, int32 Index) const;

	/** Distance (cm) from the own goal aim point to the net perimeter along Direction (0 without a goalkeeper zone). */
	float GetPerimeterDistance(const FVector& Direction) const;

	/** Goalkeeper bot: enters the defense stance inside its zone, leaves it once out of it (bGoalkeeperAutoDefense). */
	void UpdateGoalkeeperStance(ACharacter* Me);

	// ---- Breach (bAllowBreach) ----

	/** Wall the bot is breaking, the point of the straight line where it was hit, and since when. */
	TWeakObjectPtr<AActor> BreachWall;
	FVector BreachPoint = FVector::ZeroVector;
	double BreachStartTime = 0.0;

	/** Walls the bot gave up on, with the time they can be tried again. */
	TMap<TWeakObjectPtr<AActor>, double> IgnoredBreachWalls;

	/**
	 * Called with each new path: compares the path (PathLength cm, bPartial: no complete path) with the straight line to Target through the
	 * destructible walls, and sets BreachWall when the straight line is faster. Clears it otherwise.
	 */
	void EvaluateBreach(ACharacter* Me, const FVector& Target, float PathLength, bool bPartial);

	/** Walks to the run-up distance of BreachWall and Dashes into it. */
	void TickBreach(ACharacter* Me);

	/** True when Actor can be broken by the bots: a Geometry Collection (destructible wall) or a locked door. */
	static bool IsBreakable(const AActor* Actor);

	/** True when the bot may Dash into a breach wall now (BreachDashCooldown, state, energy). */
	bool CanDashNow() const;

	/** Chases Target (predicted spot) and Dashes on it when EvaluateDash allows it; steps back when too close for the run-up. */
	void ChaseAndDash(ACharacter* Me, APawn* Target);

	/** True when the ball is past GoalkeeperAdvanceFieldShare of the way from the own goal to the opponent goal (enemy territory). */
	bool IsBallInEnemyTerritory() const;

	/** Where the defenders look: the opponent carrier, else the ball. False when there is neither. */
	bool GetThreatLocation(FVector& OutLocation) const;

	/** Moves to Spot; once there, turns toward LookAt. */
	void HoldSpot(ACharacter* Me, const FVector& Spot, const FVector& LookAt, float DeltaSeconds);

	/** Runs to the support spot ahead of Anchor (the carrier), toward the opponent goal. */
	void MoveToSupportSpot(ACharacter* Me, const FVector& Anchor);

	/** Turns and Dashes on Target when EvaluateDash allows it. Returns true when the Dash was sent. */
	bool TryDashOn(ACharacter* Me, APawn* Target);
	void TickAttack(ACharacter* Me, float DeltaSeconds);
	void TickCharge(ACharacter* Me, float DeltaSeconds);

	/** Follows the NavMesh toward Target (straight line without NavMesh). Returns true once within AcceptanceRadius. */
	bool MoveTowardLocation(ACharacter* Me, const FVector& Target);

	/** Recomputes PathPoints toward Target. */
	void RebuildPath(ACharacter* Me, const FVector& Target);

	/** Turns toward Direction at AimTurnSpeed. Returns the remaining angle in degrees. */
	float TurnToward(ACharacter* Me, const FVector& Direction, float DeltaSeconds);

	/** Aim direction of the current shot (goal center plus the jitter). */
	FVector GetShotDirection(const ACharacter* Me, const UWB_TeamGoalComponent* Goal);

	void RefreshBall(const ACharacter* Me);
	void ResetBrain();
	void DrawDebug(const ACharacter* Me) const;
};
