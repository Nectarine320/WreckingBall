#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerHitProfile.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;

/**
 * How a player reacts to hits (Phase C, step 10): stun threshold, durations, knockback, grace, friendly fire, drop and hit-react animations.
 * Used by UWB_PlayerHitComponent (DA_PlayerHit_Default). A Stunned player cannot move (UWB_PlayerStateProfile::StatesBlockingMovement).
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerHitProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Minimum speed in cm/s of a thrown object at impact to stun the player. Slower hits do nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float StunMinImpactSpeed = 600.f;

	/** Seconds from the hit to the start of the get-up (enter + loop). The player is back to Free after this plus the length of StunGetUpAnim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun", meta = (ClampMin = "0", Units = "Seconds"))
	float StunDuration = 1.f;

	/** True: the soccer ball can stun a player like a thrown object. False (default): the ball never stuns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun")
	bool bBallCanStun = false;

	/** Seconds the player lies on the ground after landing from the Dash bump, before the get-up starts. Back to Free after this plus the length of DownGetUpAnim. The bump itself (airborne) is not counted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down", meta = (ClampMin = "0", Units = "Seconds"))
	float DownDuration = 1.5f;

	/** Horizontal speed in cm/s of the Dash bump: perpendicular to the Dash, toward the side of the Dash line where the player stands (scripted push, no ragdoll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DownKnockbackStrength = 600.f;

	/** Upward speed in cm/s of the Dash bump: the player is airborne (IsFalling) during the bump, and the Down timer starts on landing. 0: no jump, the Down timer starts at the hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DownKnockbackUpStrength = 200.f;

	/** Seconds after getting up during which the player cannot be stunned or knocked Down again (no fall loop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down", meta = (ClampMin = "0", Units = "Seconds"))
	float PostDownGraceSeconds = 1.f;

	/** Overlay material blinked on the character mesh during the post-Down grace (M_PlayerGraceFlash). Empty: no blink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down")
	TObjectPtr<UMaterialInterface> GraceBlinkMaterial;

	/** Seconds between two blink toggles during the post-Down grace (the flash is on for one interval, off for the next). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Down", meta = (ClampMin = "0.02", Units = "Seconds"))
	float GraceBlinkInterval = 0.1f;

	/** Seconds during which the same source (object or dasher) cannot hit this player again. Filters the multiple hits of a single contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Rules", meta = (ClampMin = "0", Units = "Seconds"))
	float HitCooldown = 0.5f;

	/** True (default): a teammate's thrown object or Dash can stun or knock Down the player. False: hits from the same team are ignored. A player is never hit by their own thrown object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Rules")
	bool bFriendlyFire = true;

	/** Horizontal speed in cm/s given to the held object (or ball) dropped when the player is stunned or knocked Down, along the hit direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Drop", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DropSpeed = 250.f;

	/** Upward speed in cm/s added to the dropped object, so it pops up a little instead of sliding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Drop", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DropUpSpeed = 200.f;

	/** Random yaw in degrees (plus or minus) applied to the drop direction, so drops do not all go the same way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Drop", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float DropRandomYaw = 45.f;

	/** Bounce distance as a fraction of the distance the Dash travelled before the impact (0.75: a 3 m Dash bounces back 2.25 m). The bounce mirrors the Dash on the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", ClampMax = "2"))
	float RecoilDistanceRatio = 0.75f;

	/** Shortest bounce in cm, used when the Dash hit the surface almost right away (little run-up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", Units = "Centimeters"))
	float RecoilMinDistance = 50.f;

	/** Upward speed in cm/s of the Dash bounce: a small hop so the bounce does not drag on the floor. 0: stays on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", ClampMax = "800", Units = "CentimetersPerSecond"))
	float RecoilUpSpeed = 150.f;

	/** Seconds the bounce slides (fast, slowing down at the end). Also how long the player stays in Recoiling (no action, no movement) after a side bounce, then back to Free. A head-on bounce stuns instead (StunDuration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", Units = "Seconds"))
	float RecoilDuration = 0.4f;

	/** Head-on limit in degrees, measured flat between the Dash direction and the surface (inverse of its normal). At or below: the bounce also stuns the player. Above: side bounce, Recoiling only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float RecoilStunAngle = 20.f;

	/** Surfaces whose normal has a vertical part above this value (0..1) give no bounce: floor, ceiling, top of an edge. Keeps the Dash from launching the player up or down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil", meta = (ClampMin = "0", ClampMax = "1"))
	float RecoilMaxNormalZ = 0.7f;

	/** Full-body bounce animation played by the AnimBP while Recoiling (AS_Dash_Recoil). Keep it about as long as RecoilDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Recoil")
	TObjectPtr<UAnimSequenceBase> RecoilAnim;

	/** Played when the player becomes Stunned (AS_Stunned_Enter). Only its length is read here: the Enter phase lasts this long. Empty: no Enter phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Animation")
	TObjectPtr<UAnimSequenceBase> StunEnterAnim;

	/** Get-up at the end of Stunned (AS_Stunned_GetUp). Its length is added to StunDuration; the player stays Stunned (cannot move) while it plays. Empty: no get-up time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Animation")
	TObjectPtr<UAnimSequenceBase> StunGetUpAnim;

	/** Played when the player is knocked Down (AS_Down_Enter). The Enter phase lasts this long, and at least until the bump lands. Empty: Enter until landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Animation")
	TObjectPtr<UAnimSequenceBase> DownEnterAnim;

	/** Get-up at the end of Down (AS_Down_GetUp). Its length is added to DownDuration; the player stays Down (invulnerable, cannot move) while it plays. Empty: no get-up time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Animation")
	TObjectPtr<UAnimSequenceBase> DownGetUpAnim;
};
