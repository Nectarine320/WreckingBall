#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "WB_PlayerHitComponent.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;
class UPrimitiveComponent;

/** Where a Stunned or Down player is in its hit reaction. Read by the AnimBP to chain the enter, loop and get-up animations. */
UENUM(BlueprintType)
enum class EWB_HitReactPhase : uint8
{
	/** Not Stunned nor Down. */
	None,
	/** Just hit: falling on the backside (Stunned) or bumped through the air (Down). Lasts the length of the profile's enter animation. */
	Enter,
	/** On the ground, waiting for the get-up (stun loop, or lying Down). */
	Loop,
	/** Getting up: the last part of the state, as long as the profile's get-up animation. Still Stunned / Down meanwhile. */
	GetUp
};
class UWB_PlayerHitProfile;
class UWB_PlayerStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWB_OnHitReceived, AActor*, Source, FGameplayTag, NewState, FVector, Direction);

/**
 * Makes a player react to hits (Phase C, step 10): a thrown object above the speed threshold stuns, a Dash knocks Down.
 * Added to BP_GenericCharacter next to UWB_PlayerStateComponent, which it drives. The Blueprint detects the hit
 * (capsule Hit for thrown objects, Dash sweep for dashes) and calls ReceiveThrownHit or ReceiveDashHit.
 * At BeginPlay, turns on the physics hit events of the owner's root (capsule).
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerHitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerHitComponent();

	/** Hit rules and durations (DA_PlayerHit_Default). Empty: the class defaults of UWB_PlayerHitProfile are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit")
	TObjectPtr<UWB_PlayerHitProfile> HitProfile;

	/** Team of the player, set by BP_SoccerGamemode when the player is registered. -1: no team, every hit counts as an enemy hit. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "PlayerHit")
	int32 TeamIndex = -1;

	/** Called after a hit was accepted: NewState is Player.State.Stunned or Player.State.Down, Direction is the flat push direction. Hook for the drop (ForceDrop) and the feedback. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerHit")
	FWB_OnHitReceived OnHitReceived;

	/**
	 * A thrown object (or the ball) hit the player at ImpactVelocity. Stuns the player when the speed reaches StunMinImpactSpeed
	 * and the hit is allowed (not its own object, friendly fire, cooldown, grace, ball rule, state table). Returns true when stunned.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerHit", meta = (ReturnDisplayName = "Stunned"))
	bool ReceiveThrownHit(AActor* Source, FVector ImpactVelocity, bool bSourceIsBall);

	/**
	 * A dashing player hit this player (Direction: the Dash direction). When the hit is allowed (friendly fire, cooldown, grace, state table):
	 * enters Down, faces and bumps the player sideways (perpendicular to the Dash, airborne), then starts the DownDuration timer on landing.
	 * The minimum run-up is checked by the dasher before the call. Returns true when knocked Down.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerHit", meta = (ReturnDisplayName = "KnockedDown"))
	bool ReceiveDashHit(AActor* Dasher, FVector Direction);

	/**
	 * True when a Dash of Dasher would knock this player Down now: same rules as ReceiveDashHit (friendly fire, cooldown, grace, state table),
	 * without the run-up check. Lets a bot skip a Dash that cannot work.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerHit", meta = (ReturnDisplayName = "CanBeKnockedDown"))
	bool CanBeDashedBy(AActor* Dasher) const;

	/**
	 * The Dash of this player hit a surface too heavy to break (HitActor: the surface, HitNormal: the sweep impact normal, DashDirection: the Dash direction,
	 * RunUpDistance: cm travelled by the Dash before the impact). Only while Dashing, never on another player, and not on a mostly vertical normal (RecoilMaxNormalZ).
	 * Head-on (angle at or below RecoilStunAngle): Stunned, with the stun durations and animations, and OnHitReceived is broadcast. Side hit: Recoiling for RecoilDuration.
	 * In both cases the player turns away from the surface and slides along the Dash direction mirrored on the surface, over RunUpDistance x RecoilDistanceRatio
	 * (at least RecoilMinDistance) during RecoilDuration, with a small hop (RecoilUpSpeed). Returns true when the bounce happened: the Blueprint must then stop
	 * the Dash timeline and end the pass-through itself. Stunned: true when the bounce also stunned the player.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerHit", meta = (ReturnDisplayName = "Recoiled"))
	bool ApplyDashRecoil(AActor* HitActor, FVector HitNormal, FVector DashDirection, float RunUpDistance, bool& bStunned);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Speed of a simulated object just before it hit (the Hit event gives the velocity after the bounce).
	 * Same method as the destructibles: velocity plus or minus NormalImpulse / mass, the larger one is kept. Not simulated: its current velocity.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerHit")
	static FVector EstimatePreImpactVelocity(UPrimitiveComponent* OtherComp, FVector NormalImpulse);

	/** True while the player is Down and during PostDownGraceSeconds after getting up: no stun, no Down. The mesh blinks white meanwhile (GraceBlinkMaterial). */
	UFUNCTION(BlueprintPure, Category = "PlayerHit")
	bool IsInPostDownGrace() const;

	/** Phase of the current hit reaction (None when not Stunned nor Down). Read every frame by the AnimBP to pick the enter, loop or get-up animation. */
	UFUNCTION(BlueprintPure, Category = "PlayerHit")
	EWB_HitReactPhase GetHitReactPhase() const;

	/**
	 * Velocity change to give to the object the player drops when hit (BPC_GrabObject ForceDrop): DropSpeed along HitDirection (flat),
	 * turned by a random yaw up to DropRandomYaw, plus DropUpSpeed upward. Zero HitDirection: the player's forward is used.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayerHit")
	FVector MakeDropImpulse(FVector HitDirection) const;

	/** The profile in use: HitProfile, or the UWB_PlayerHitProfile class defaults when it is empty. Never null. */
	UFUNCTION(BlueprintPure, Category = "PlayerHit")
	const UWB_PlayerHitProfile* GetActiveHitProfile() const;

protected:
	virtual void BeginPlay() override;

private:
	/** State component of the owner, found at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerStateComponent> PlayerState;

	/** Game time of the last accepted hit per source, for HitCooldown. */
	TMap<TWeakObjectPtr<AActor>, double> LastHitTimes;

	/** Game time until which the player cannot be hit (post-Down grace). */
	double GraceEndTime = -1.0;

	/** Common rules of both hit types: valid state component, not self, friendly fire, cooldown, grace. */
	bool CanBeHitBy(AActor* Source) const;

	/** Records Source for HitCooldown. */
	void RegisterHit(AActor* Source);

	/** True between a Dash hit and the landing of the bump: the Down timer starts when the player touches the ground. */
	bool bAwaitingDownLanding = false;

	/** Game time of the hit that started the current Stunned / Down (start of the Enter phase). */
	double HitReactStartTime = -1.0;

	/** Game time when the get-up starts (GetUp phase). -1 while unknown (Down still in the air) or outside a hit reaction. */
	double GetUpStartTime = -1.0;

	/** Length in seconds of Anim, 0 when empty. */
	static float GetAnimLength(const UAnimSequenceBase* Anim);

	/** Down on the ground: back to Free after DownDuration plus the get-up length, and sets GetUpStartTime. */
	void StartDownGroundTimer();

	/** True while the Dash bounce slides the player (TickComponent moves it). */
	bool bBouncing = false;

	/** State the bounce put the player in (Recoiling or Stunned): leaving it stops the slide. */
	FGameplayTag BounceState;

	/** Bounce start location, flat direction, length in cm, elapsed and total time in seconds. */
	FVector BounceStart = FVector::ZeroVector;
	FVector BounceDirection = FVector::ZeroVector;
	float BounceDistance = 0.f;
	float BounceElapsed = 0.f;
	float BounceDuration = 0.f;

	/** Ends the bounce slide and turns the tick off. */
	void StopBounce();

	/** Toggles the grace blink overlay (GraceBlinkInterval). */
	FTimerHandle GraceBlinkTimer;

	/** Overlay material of the mesh before the blink started, restored at the end. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OverlayBeforeBlink;

	/** True while the flash overlay is shown. */
	bool bGraceFlashOn = false;

	/** Starts blinking the mesh until GraceEndTime (no material in the profile: nothing). */
	void StartGraceBlink();

	/** One blink step: toggles the flash, or stops and restores the overlay once the grace is over. */
	void TickGraceBlink();

	/** Stops the blink and restores the original overlay. */
	void StopGraceBlink();

	/** Starts the post-Down grace when the player leaves Down. */
	UFUNCTION()
	void HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState);

	/** End of the Dash bump: starts the Down timer (DownDuration) once the player is back on the ground. */
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);
};
