#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WB_BallPossessionSubsystem.generated.h"

class UPrimitiveComponent;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWB_OnBallPossessionChanged, AActor*, Ball, AActor*, NewPossessor, AActor*, OldPossessor);

/** Kick record of one ball: who kicked it, when, and whether it is still in kicked mode. */
USTRUCT()
struct FWB_BallKickRecord
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Kicker;
	double KickTime = 0.0;
	float ReleaseSpeed = 0.f;
	bool bKicked = false;
	/** True once the kicked ball rebounded (its flat direction turned sharply in one frame). */
	bool bRebounded = false;
	bool bHasLastVelocity = false;
	FVector LastVelocity = FVector::ZeroVector;
};

/** Speed trail of one ball: the Niagara component to switch, its thresholds (cm/s) and its state. */
USTRUCT()
struct FWB_BallTrailRecord
{
	GENERATED_BODY()

	TWeakObjectPtr<UNiagaraComponent> Trail;
	float StartSpeed = 0.f;
	float StopSpeed = 0.f;
	float ImpactSpeed = 0.f;
	bool bOn = false;
	bool bHasLastVelocity = false;
	FVector LastVelocity = FVector::ZeroVector;
};

/**
 * Ball possession rules shared by every player (lot 11.1, 2026-11-17): kicked mode and kicker re-catch delay.
 * A kicked ball cannot be caught by walking into it (only by the defense stance) until its speed drops under its
 * KickedReleaseSpeed (PDA_PhysicsProfile, passed by MarkBallKicked). The kicker cannot catch its own ball again for a short delay.
 */
UCLASS()
class WRECKINGBALL_API UWB_BallPossessionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Called when the dribbler of a ball changes: a player catches it (NewPossessor set) or loses, passes or kicks it (NewPossessor None).
	 * A direct steal fires once, with both possessors set. Bind through Get WB_BallPossessionSubsystem.
	 */
	UPROPERTY(BlueprintAssignable, Category = "BallPossession")
	FWB_OnBallPossessionChanged OnBallPossessionChanged;

	/**
	 * Records a kick of Ball by Kicker (call right after the kick impulse). The ball enters kicked mode until its speed drops under
	 * KickedReleaseSpeed (cm/s, from the ball's PDA_PhysicsProfile). Also starts the kicker re-catch delay, even for a weak kick.
	 */
	UFUNCTION(BlueprintCallable, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static void MarkBallKicked(const UObject* WorldContextObject, AActor* Ball, AActor* Kicker, float KickedReleaseSpeed);

	/** Ends the kicked mode of Ball and forgets its kicker (the ball was caught). */
	UFUNCTION(BlueprintCallable, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static void ClearBallKick(const UObject* WorldContextObject, AActor* Ball);

	/** True while Ball is in kicked mode (only the defense stance can catch it). */
	UFUNCTION(BlueprintPure, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static bool IsBallKicked(const UObject* WorldContextObject, const AActor* Ball);

	/** True when a player holds Ball: a player dribbles it (UWB_BallDribbleComponent), or one of the players' Physics Handles grabs it. */
	UFUNCTION(BlueprintPure, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static bool IsBallHeld(const UObject* WorldContextObject, const AActor* Ball);

	/** Player who possesses Ball: its dribbler, or the pawn whose Physics Handle grabs it. None when the ball is free. */
	UFUNCTION(BlueprintPure, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetBallPossessor(const UObject* WorldContextObject, const AActor* Ball);

	/** Records Player as the dribbler of Ball (called by UWB_BallDribbleComponent::StartDribble). */
	static void SetBallDribbler(const UObject* WorldContextObject, AActor* Ball, AActor* Player);

	/** Forgets the dribbler of Ball, only if it is still Player (called by UWB_BallDribbleComponent::StopDribble). */
	static void ClearBallDribbler(const UObject* WorldContextObject, const AActor* Ball, const AActor* Player);

	/**
	 * Hands the speed trail of Ball to the subsystem (call once, at BeginPlay). Each frame, the trail turns on when the ball goes faster than
	 * StartSpeed, or when a collision changes its velocity by at least ImpactSpeed in one frame (kick, Dash, wall, thrown object...); it turns
	 * off when the ball drops under StopSpeed, and never shows while a player holds the ball. Values in cm/s, from the ball's PDA_PhysicsProfile.
	 * The switch is the Niagara user float "TrailActive" (1 on, 0 off).
	 */
	UFUNCTION(BlueprintCallable, Category = "BallPossession", meta = (WorldContext = "WorldContextObject"))
	static void RegisterBallTrail(const UObject* WorldContextObject, AActor* Ball, UNiagaraComponent* Trail, float StartSpeed, float StopSpeed, float ImpactSpeed);

	/**
	 * True when Player may catch Ball by walking into it: not held, not kicked (or kicked, rebounded and slower than ReboundReleaseSpeed when it is
	 * above 0), and not its own kick within KickerRecatchDelay seconds. OutRefusal (optional) receives why it is refused, for the debug log.
	 */
	bool CanBeCaughtByWalking(const AActor* Ball, const AActor* Player, float KickerRecatchDelay, float ReboundReleaseSpeed = 0.f, FString* OutRefusal = nullptr) const;

	/** Body of Ball whose velocity is read (simulated root, or first simulated primitive). */
	static UPrimitiveComponent* FindBallBody(const AActor* Ball);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	TMap<TWeakObjectPtr<AActor>, FWB_BallKickRecord> Records;
	TMap<TWeakObjectPtr<AActor>, FWB_BallTrailRecord> Trails;

	/** Ball -> player who dribbles it. */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> Dribblers;

	void TickTrails();

	static UWB_BallPossessionSubsystem* Get(const UObject* WorldContextObject);
};
