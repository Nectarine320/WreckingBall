#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_PlayerCommandComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnMoveCommand, float, XAxis, float, YAxis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWB_OnPlayerCommand);

/**
 * Single entry point for the commands of a player (bot AI today, possibly the human controller later).
 * Added to BP_GenericCharacter. It does not act by itself: each command fires a dispatcher that the Blueprint
 * binds to the matching Request* function (RequestMove, RequestDash, RequestGrab, RequestKickOrThrow, ReleaseKickOrThrow),
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

private:
	bool bMoving = false;
};
