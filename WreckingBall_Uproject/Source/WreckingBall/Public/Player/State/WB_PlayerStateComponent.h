#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WB_PlayerStateComponent.generated.h"

class UWB_PlayerStateProfile;
class UWB_PlayerEnergyProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnPlayerStateChanged, FGameplayTag, OldState, FGameplayTag, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnHoldingChanged, FGameplayTag, OldHolding, FGameplayTag, NewHolding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnActionRefused, FGameplayTag, RequestedState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnBallCatchRequested, AActor*, Ball);

/**
 * Single source of truth of what a player does (Player.State.*) and holds (Player.Holding.*), Phase C step 9.
 * Added to BP_GenericCharacter. The two values are read-only from Blueprint: they change only through
 * ChangePlayerState (checked against the profile's transition table) and ChangeHolding, which fire the dispatchers.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerStateComponent();

	/** What the player is doing (Player.State.*). Change it only with ChangePlayerState. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PlayerState", meta = (Categories = "Player.State"))
	FGameplayTag CurrentState;

	/** What the player holds (Player.Holding.*). Change it only with ChangeHolding. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PlayerState", meta = (Categories = "Player.Holding"))
	FGameplayTag CurrentHolding;

	/** State rules and durations (DA_PlayerState_Default). Required: empty, every state change is refused and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState")
	TObjectPtr<UWB_PlayerStateProfile> PlayerStateProfile;

	/** Energy settings of the player: sprint, recharge, exhaustion, action costs (DA_PlayerEnergy_Default). Required: empty, a warning is logged and the Blueprint reads None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState")
	TObjectPtr<UWB_PlayerEnergyProfile> EnergyProfile;

	/** Called after CurrentState changed. Bind it instead of reading CurrentState every frame (HUD, AnimBP). */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState")
	FWB_OnPlayerStateChanged OnPlayerStateChanged;

	/** Called after CurrentHolding changed. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState")
	FWB_OnHoldingChanged OnHoldingChanged;

	/** Called when a player request (TryEnterState) is refused, e.g. a Dash while holding something. No listener yet: hook for a future sound or icon. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState")
	FWB_OnActionRefused OnActionRefused;

	/**
	 * Fired when a free ball enters the catch zone in front of the player (Free state, empty hands, ball neither held nor kicked,
	 * kicker re-catch delay over). Bind to the ball pickup (BPC_GrabObject.PickUpSoccerBall). Fired again every short while as long as it lasts.
	 */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState|Possession")
	FWB_OnBallCatchRequested OnBallCatchRequested;

	/** Kick / throw charge from 0 (just pressed) to 1 (full charge), set by BPC_GrabObject while Kicking or Throwing. Slows the player down (GetMoveSpeedMultiplier). Back to 0 when the state ends. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PlayerState")
	float ActionCharge = 0.f;

	/** Shows the current state and holding above the player (development builds only). Off by default. Use SetDebugShowState to toggle it during play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebugShowState = false;

	/** Height in cm above the actor location where the debug text is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (Units = "Centimeters"))
	float DebugTextHeight = 120.f;

	/**
	 * Tries to enter NewState. Refused (returns false, nothing changes) when CanEnterState is false.
	 * On success, fires OnPlayerStateChanged(OldState, NewState). The only way to change CurrentState.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Changed"))
	bool ChangePlayerState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState);

	/**
	 * Sets what the player holds. Returns false when NewHolding is not a Player.Holding.* tag or is already held.
	 * On success, fires OnHoldingChanged(OldHolding, NewHolding). The only way to change CurrentHolding.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Changed"))
	bool ChangeHolding(UPARAM(meta = (Categories = "Player.Holding")) FGameplayTag NewHolding);

	/**
	 * Player request (Request* events): same as ChangePlayerState, but fires OnActionRefused when refused.
	 * Use ChangePlayerState for system changes (GameMode, end of an action) that are not player requests.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Entered"))
	bool TryEnterState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState);

	/**
	 * Enters NewState (same rules as ChangePlayerState), then goes back to Free after Duration seconds if the player is still in NewState.
	 * Used for the fallback end of Kicking / Throwing and for Celebrating / Sulking. Any other state change cancels the timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Changed"))
	bool ChangePlayerStateForDuration(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState, float Duration);

	/**
	 * Keeps the current state for Duration seconds, then goes back to Free if the state did not change meanwhile.
	 * Used to end Kicking / Throwing after the release when the animation has no Release notify (KickFallbackDuration / ThrowFallbackDuration).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	void ReturnToFreeAfter(float Duration);

	/** True when the move and look inputs have an effect (CurrentState is not in the profile's StatesBlockingMovement). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool CanMove() const;

	/** True when the sprint input has an effect (CurrentState is not in the profile's StatesBlockingSprint). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool CanSprint() const;

	/**
	 * Scale to apply to the move input: 0 when CanMove is false (Disabled, Down, Stunned by default),
	 * from 1 to ActionMoveSpeedMultiplier with ActionCharge while Kicking or Throwing, 1 otherwise (the defense stance speeds are in the dive profile).
	 * Times HoldingBallMoveSpeedMultiplier of the state profile while holding the ball (possession penalty).
	 * Scaling the input (not MaxWalkSpeed) keeps the sprint system untouched.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	float GetMoveSpeedMultiplier() const;

private:
	/** Part of GetMoveSpeedMultiplier given by the current state alone (kick / throw charge), without the possession penalty. */
	float GetStateMoveSpeedMultiplier() const;

public:

	/**
	 * Stick direction in world space for the forced run while Celebrating (call it from RequestMove instead of Add Movement Input).
	 * The component runs forward at full speed every frame and turns toward this direction at the profile's CelebrationTurnRate. Zero: runs straight.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	void SetCelebrationSteering(FVector WorldDirection);

	/** Sets ActionCharge (clamped 0-1). Ignored outside Kicking / Throwing. */
	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	void SetActionCharge(float Charge);

	/** True when ChangePlayerState(NewState) would succeed: allowed by the transition table from CurrentState, and the hand rules (empty hands / holding) are met. */
	UFUNCTION(BlueprintPure, Category = "PlayerState", meta = (ReturnDisplayName = "Allowed"))
	bool CanEnterState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState) const;

	/** True when CurrentState is State (or a child of it). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool IsInState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag State) const;

	/** True when the player holds something (CurrentHolding is not Player.Holding.None). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool IsHoldingAnything() const;

	/** The state profile (PlayerStateProfile). Null when it is not assigned (a warning is logged once). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	const UWB_PlayerStateProfile* GetActiveProfile() const;

	/** The energy profile (EnergyProfile). Null when it is not assigned (a warning is logged once). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	const UWB_PlayerEnergyProfile* GetActiveEnergyProfile() const;

	/** Turns the debug text above the player on or off during play. */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetDebugShowState(bool bShow);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Timer that brings the player back to Free (ChangePlayerStateForDuration, ReturnToFreeAfter). */
	FTimerHandle StateDurationTimer;

	/** Last stick direction received while Celebrating (flat, normalized, or zero). */
	FVector CelebrationSteering = FVector::ZeroVector;

	/** Walk speed of the owner's movement at BeginPlay (WalkSpeedDefault, set by the construction script). Base of the celebration ramp and value restored after it. 0: no character movement. */
	float DefaultWalkSpeed = 0.f;

	/** Game time when Celebrating started, for the speed ramp. */
	double CelebrationStartTime = 0.0;

	/** Missing profile warnings already logged (one per profile and component). */
	mutable bool bWarnedNoStateProfile = false;
	mutable bool bWarnedNoEnergyProfile = false;

	/** Sets the owner's MaxWalkSpeed to DefaultWalkSpeed x Multiplier (no effect without character movement). */
	void ApplyWalkSpeedMultiplier(float Multiplier) const;

	/** True when CurrentState is Kicking or Throwing. */
	bool IsInChargedAction() const;

	/** Ticks only for the debug text or the Celebrating run. */
	void RefreshTickEnabled();

	/** True when the automatic ball catch is active: state profile with a BallClass, Free state, empty hands. */
	bool CanCatchBallNow() const;

	/** Looks for a catchable ball in the catch zone and fires OnBallCatchRequested. */
	void TickBallCatch();

	/** Next time a catch request may be fired again (the pickup refuses some balls). */
	double NextCatchRequestTime = 0.0;

	/** Forced run of the Celebrating state, called every frame. */
	void TickCelebrationRun(float DeltaTime);
};
