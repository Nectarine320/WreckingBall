#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_PlayerHitProfile.generated.h"

class UMaterialInterface;

/**
 * How a player reacts to hits (Phase C, step 10): stun threshold, durations, knockback, grace and friendly fire.
 * Used by UWB_PlayerHitComponent (DA_PlayerHit_Default). The stun slowdown lives in UWB_PlayerStateProfile (StunMoveSpeedMultiplier).
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_PlayerHitProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Minimum speed in cm/s of a thrown object at impact to stun the player. Slower hits do nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float StunMinImpactSpeed = 600.f;

	/** Seconds the player stays Stunned before going back to Free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun", meta = (ClampMin = "0", Units = "Seconds"))
	float StunDuration = 1.f;

	/** True: the soccer ball can stun a player like a thrown object. False (default): the ball never stuns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerHit|Stun")
	bool bBallCanStun = false;

	/** Seconds the player stays Down on the ground after landing from the Dash bump, before getting up (back to Free). The bump itself (airborne) is not counted. */
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
};
