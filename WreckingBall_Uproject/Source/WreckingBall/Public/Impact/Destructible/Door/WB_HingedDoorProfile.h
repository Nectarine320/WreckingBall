#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_HingedDoorProfile.generated.h"

/**
 * Tuning of a hinged door (UWB_HingedDoorComponent, DA_HingedDoor_Default): hinge, saloon spring, lock and tear off.
 * The door health (how hard it must be hit to be torn off its hinges) is NOT here: it is the MaxHealth of the door's
 * destructible profile (PDA_DestructibleProfile), like every destructible. When it reaches 0, the door is torn off.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_HingedDoorProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Hinge ----

	/** How far the door swings each way from closed, in degrees (100 = a bit past perpendicular, both directions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Hinge", meta = (ClampMin = "1", ClampMax = "170", Units = "Degrees"))
	float SwingLimitAngle = 100.f;

	/** Mass of the door in kg. Heavier: players and the ball push it less, and it hits harder when it swings back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Hinge", meta = (ClampMin = "1", Units = "Kilograms"))
	float DoorMass = 25.f;

	/** Angular damping of the door body: slows down every swing (air, hinge friction). Higher: the door stops sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Hinge", meta = (ClampMin = "0"))
	float AngularDamping = 1.f;

	/** Linear damping of the door body. Mostly matters once the door is torn off and falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Hinge", meta = (ClampMin = "0"))
	float LinearDamping = 0.1f;

	// ---- Spring ----

	/** True: a saloon spring brings the door back to closed. False: the door stays where it is pushed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Spring")
	bool bUseSpring = true;

	/** Strength of the spring that pulls the door closed (acceleration drive, independent of the mass). Higher: snaps back faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Spring", meta = (EditCondition = "bUseSpring", EditConditionHides, ClampMin = "0"))
	float SpringStiffness = 50.f;

	/** Damping of the spring. Low: the door swings back and forth a few times before it settles (saloon). High: it closes without overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Spring", meta = (EditCondition = "bUseSpring", EditConditionHides, ClampMin = "0"))
	float SpringDamping = 6.f;

	// ---- Lock ----

	/** True: doors using this profile can start locked (Starts Locked on the door). A locked door does not move and acts like a wall until it is kicked open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Lock")
	bool bCanBeLocked = false;

	/** Damage the lock takes before it breaks (Dash, ball, thrown object). The door health goes down at the same time: keep the door MaxHealth higher so the lock breaks first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Lock", meta = (EditCondition = "bCanBeLocked", EditConditionHides, ClampMin = "0"))
	float LockHealth = 60.f;

	/** Speed in degrees per second the door swings open at when its lock breaks, away from the hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Lock", meta = (EditCondition = "bCanBeLocked", EditConditionHides, ClampMin = "0", Units = "DegreesPerSecond"))
	float KickOpenAngularSpeed = 400.f;

	// ---- Tear off ----

	/** Speed in cm/s given to the door, away from the hit, when it is torn off its hinges (door health at 0). 0: it just falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|TearOff", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float TearOffSpeed = 300.f;
};
