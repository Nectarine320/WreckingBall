#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_PlayerCommandComponent.generated.h"

class UWB_PlayerDashProfile;
class UWB_PassProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnMoveCommand, float, XAxis, float, YAxis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWB_OnPlayerCommand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnPassCommand, APawn*, Receiver, FVector, BallVelocity);

/**
 * Single entry point for the commands of a player (bot AI today, possibly the human controller later).
 * Added to BP_GenericCharacter. It does not act by itself: each command fires a dispatcher that the Blueprint
 * binds to the matching Request* function (RequestMove, RequestDash, RequestGrab, RequestKickOrThrow, ReleaseKickOrThrow, pass),
 * so a bot follows exactly the same rules as a human player (states, energy, refusals).
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerCommandComponent();

	/** Move command, as stick axes relative to the shared camera (X: right, Y: forward). Bind to RequestMove. Fired every frame while moving, then once with zero when stopping. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand")
	FWB_OnMoveCommand OnMoveCommand;

	/** Dash command. Bind to RequestDash. The Dash goes along the actor forward: call SetFacingDirection first. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand")
	FWB_OnPlayerCommand OnDashCommand;

	/** Grab command (ball or physics object in front of the player). Bind to RequestGrab. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand")
	FWB_OnPlayerCommand OnGrabCommand;

	/** Kick / throw press: starts the charge. Bind to RequestKickOrThrow. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand")
	FWB_OnPlayerCommand OnKickPressCommand;

	/** Kick / throw release: sends the held ball or object along the actor forward. Bind to ReleaseKickOrThrow. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand")
	FWB_OnPlayerCommand OnKickReleaseCommand;

	/**
	 * Pass command: the Blueprint releases the dribbled ball and sets its velocity to BallVelocity (cm/s, flat), toward Receiver.
	 * Fired by RequestPass and RequestDirectionalPass. Receiver is None for a free pass (aim assist, no teammate in the cone).
	 */
	UPROPERTY(BlueprintAssignable, Category = "PlayerCommand|Pass")
	FWB_OnPassCommand OnPassCommand;

	/** Pass settings (DA_Pass_Default). Required for RequestPass: empty, the pass is refused and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerCommand|Pass")
	TObjectPtr<UWB_PassProfile> PassProfile;

	/**
	 * Passes the dribbled ball to Receiver along the ground (PassProfile): turns the owner toward the pass, then fires OnPassCommand.
	 * Refused when the owner does not hold the ball or is not Free (charging a kick, Dashing...). Returns true when the ball left the owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand|Pass")
	bool RequestPass(APawn* Receiver, float Charge = 0.f);

	/**
	 * Passes the dribbled ball in Direction (world, flat; zero: the owner's facing). PassProfile->bUseAimAssist on: to the teammate within
	 * AimAssistAngle of Direction, else a free pass along Direction (OutReceiver None). Off: to the teammate best aligned with Direction.
	 * Same refusals as RequestPass. Charge (0..1, charged pass): faster pass and, when strong, a small lift (PassProfile). Returns true when the ball left the owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand|Pass")
	bool RequestDirectionalPass(FVector Direction, APawn*& OutReceiver, float Charge = 0.f);

	/**
	 * Flat ball velocity (cm/s) of a pass from the dribbled ball to Receiver (PassProfile), at Charge (0..1, speed only, no lift).
	 * False when the owner dribbles no ball or a setting is missing.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Pass", meta = (ReturnDisplayName = "Valid"))
	bool ComputePassVelocity(APawn* Receiver, FVector& OutVelocity, float Charge = 0.f) const;

	/** Starts charging a pass (PassProfile->bUseChargedPass, owner holds the ball and is Free). Returns true when the charge started. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand|Pass")
	bool BeginPassCharge();

	/** Stops the pass charge without passing. Safe when not charging. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand|Pass")
	void CancelPassCharge();

	/** True between BeginPassCharge and the release (or the cancel). */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Pass")
	bool IsChargingPass() const { return bChargingPass; }

	/** Charge (0..1) of the pass being charged, from the hold time (PassProfile->GetChargeFromHoldTime). 0 when not charging. For the UI. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Pass")
	float GetPassCharge() const;

	/** True while the owner holds the ball (Player.Holding.Ball). */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand")
	bool IsHoldingBall() const;

	/** Moves toward a world direction (flattened, Scale 0-1 like a stick tilt). Converted to shared-camera stick axes, then OnMoveCommand. Call every frame. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void MoveInWorldDirection(FVector WorldDirection, float Scale = 1.f);

	/** Stops moving: fires OnMoveCommand with zero axes once if the player was moving. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void StopMoving();

	/** Fires OnDashCommand. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void Dash();

	/** Fires OnGrabCommand. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void Grab();

	/** Fires OnKickPressCommand. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void KickPress();

	/** Fires OnKickReleaseCommand. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void KickRelease();

	/** Turns the owner instantly toward a world direction (yaw only), and its controller too. Used to aim the Dash and the kick. */
	UFUNCTION(BlueprintCallable, Category = "PlayerCommand")
	void SetFacingDirection(FVector WorldDirection);

	/** Converts a world direction into stick axes relative to the shared camera (player 0 camera manager), the same axes RequestMove expects. No camera: world X forward, Y right. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand")
	void WorldDirectionToStickAxes(FVector WorldDirection, float& XAxis, float& YAxis) const;

	/** True while the last move command was not zero. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand")
	bool IsMoving() const { return bMoving; }

	/** Dash settings of the player: distance, duration, scan, run-ups, damage angle (DA_PlayerDash_Default). Read by BP_GenericCharacter and the bots. Required: empty, a warning is logged and the bots never Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerCommand")
	TObjectPtr<UWB_PlayerDashProfile> DashProfile;

	/** The Dash profile (DashProfile). Null when it is not assigned (a warning is logged once). */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand")
	const UWB_PlayerDashProfile* GetDashProfile() const;

	// ---- Readout: energy values the owner Blueprint keeps in its own variables (sprint Timelines), read by name for the bots. ----

	/** Owner Blueprint float variable holding the energy, 0 (empty) to 1 (full). BP_GenericCharacter: Sprint Lifetime Percent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerCommand|Readout")
	FName EnergyPercentVariable = TEXT("Sprint Lifetime Percent");

	/** Owner Blueprint enum variable of the walk mode. BP_GenericCharacter: WalkType (E_WalkSpeed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerCommand|Readout")
	FName WalkTypeVariable = TEXT("WalkType");

	/** Display name of the WalkTypeVariable value meaning exhausted (RequestDash refuses the Dash then). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerCommand|Readout")
	FString ExhaustedWalkType = TEXT("Exhausted");

	/** Energy of the owner, 0..1 (EnergyPercentVariable). 1 when the variable is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	float GetEnergyPercent() const;

	/** True while the owner is exhausted (WalkTypeVariable equals ExhaustedWalkType): its Dash is refused. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	bool IsExhausted() const;

	/** Energy used by one Dash (EnergyProfile of the owner's UWB_PlayerStateComponent), 0..1. 1 (a full bar) when the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	float GetDashEnergyCost() const;

	/** Distance in cm of a full Dash (DashProfile). 0 when the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	float GetDashDistance() const;

	/** Duration in seconds of a full Dash (DashProfile). 0 when the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	float GetDashDuration() const;

	/** Minimum run-up in cm for a Dash to knock a player Down (DashProfile). 0 when the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerCommand|Readout")
	float GetDashMinRunUp() const;

private:
	bool bMoving = false;

	/** Reads a float (or integer) variable of the owner by name. False (and a warning once) when it is missing. */
	bool ReadOwnerNumber(FName Variable, float& OutValue) const;

	/** Variables already reported as missing, so the warning is logged once. */
	mutable TSet<FName> WarnedVariables;

	/** True when a pass may start: the owner holds the ball and is Free. */
	bool CanPassNow() const;

	/** True when PassProfile is set (warns once otherwise). */
	bool HasPassProfile() const;

	/** Adds the lift of Charge (PassProfile->ApplyPassLoft), turns the owner toward Velocity, fires OnPassCommand and records the pass for the receiver (MarkBallPassed). */
	bool FirePass(APawn* Receiver, const FVector& Velocity, float Charge);

	/** Pass charge in progress (BeginPassCharge) and its start time. */
	bool bChargingPass = false;
	double PassChargeStartTime = 0.0;

	/** Missing PassProfile already reported. */
	mutable bool bWarnedNoPassProfile = false;

	/** Missing DashProfile already reported. */
	mutable bool bWarnedNoDashProfile = false;
};
