#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "WB_SoccerAIController_Base.generated.h"

class ACharacter;
class UWB_PlayerCommandComponent;
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
	/** The ball is free: run to it and grab it. */
	SeekBall,
	/** An opponent carries the ball: chase it and Dash on it when in range. */
	ChaseCarrier,
	/** A teammate carries the ball: run ahead of it, toward the opponent goal. */
	Support,
	/** The bot holds the ball: reach the shooting zone, aim, charge and shoot. */
	Attack
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

	/** Team of the possessed player (TeamIndex of its UWB_PlayerHitComponent). -1: no team. */
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

	/** Picks the intent of this frame. Default: Attack when holding something, ChaseCarrier / Support when another player has the ball, else SeekBall. */
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

	TWeakObjectPtr<AActor> CachedBall;
	double NextBallSearchTime = 0.0;

	/** Current path (world points) toward PathTarget, and the index of the point being followed. */
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	FVector PathTarget = FVector::ZeroVector;
	double NextRepathTime = 0.0;

	double NextGrabTime = 0.0;
	double NextDashTime = 0.0;

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

	void TickSeekBall(ACharacter* Me, float DeltaSeconds);
	void TickChaseCarrier(ACharacter* Me, float DeltaSeconds);
	void TickSupport(ACharacter* Me, float DeltaSeconds);
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
