#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_HingedDoorProfile.generated.h"

/**
 * Tuning of a hinged door (UWB_HingedDoorComponent, DA_HingedDoor_Default): hinge, saloon spring, lock and tear off.
 * The door health (how hard it must be hit to be torn off its hinges) is NOT here: it is the MaxHealth of the door's
 * destructible profile (PDA_DestructibleProfile), like every destructible. When it reaches 0, the door is torn off.
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Destruction"))
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

	/** True: doors using this profile can start locked (Starts Locked on the door). A locked door does not move and acts like a wall until its lock breaks (then it is torn off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Lock")
	bool bCanBeLocked = false;

	/** Damage the lock takes before it breaks (Dash, ball, thrown object). When it breaks, the door is torn off its hinges and becomes a loose physics block. The door health goes down at the same time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Lock", meta = (EditCondition = "bCanBeLocked", EditConditionHides, ClampMin = "0"))
	float LockHealth = 60.f;

	// ---- Dash ----

	/**
	 * Angle in degrees the door is forced open to, away from the dasher, when a player dashes through an unlocked door. The swing is driven by code
	 * (kinematic): nothing stops it; the ball and objects in the way are pushed, players in the way are knocked Down (slam). Then the door simulates again
	 * and the spring brings it back. Capped by SwingLimitAngle. Above about 95 degrees the door goes into thick blocker boxes of the wall: keep them thin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Dash", meta = (ClampMin = "0", ClampMax = "170", Units = "Degrees"))
	float DashOpenAngle = 90.f;

	/** Speed in degrees per second of the forced opening on a Dash (90 degrees at 500: about 0.18 s). Keep it above SlamMinAngularSpeed so the forced opening slams. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Dash", meta = (ClampMin = "1", Units = "DegreesPerSecond"))
	float DashKickAngularSpeed = 500.f;

	/** Seconds a dasher keeps the priority on the door: during this window, a dash from the other side is slammed by the door instead of going through. Dashes in the same frame: the strongest impact wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Dash", meta = (ClampMin = "0", Units = "Seconds"))
	float DashPriorityWindow = 0.5f;

	/** Seconds the dasher who opened the door (and anyone dashing through in the same direction) cannot be slammed by it, from the moment they hit it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Dash", meta = (ClampMin = "0", Units = "Seconds"))
	float DasherProtectionDuration = 0.8f;

	/** Speed in cm/s given to a door already torn off its hinges (loose block) when a player dashes into it, along the Dash. The dasher goes through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Dash", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float LooseDoorDashPushSpeed = 400.f;

	// ---- Slam ----

	/**
	 * Opening speed in degrees per second above which the door is "slammed": while it swings away from closed faster than this
	 * (Dash, ball, thrown object), a player it hits is knocked Down, pushed the way the door moves. As soon as the door swings back toward closed, it is harmless again.
	 * Keep it above the speed a walking player gives the door.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|Slam", meta = (ClampMin = "0", Units = "DegreesPerSecond"))
	float SlamMinAngularSpeed = 300.f;

	// ---- Tear off ----

	/**
	 * Speed in cm/s given to the door, away from the hit, when it is torn off its hinges (lock broken, door health at 0, or wall destroyed:
	 * then away from the last hit the wall received). The push is applied high on the door so it tips over. 0: it just falls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|TearOff", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float TearOffSpeed = 300.f;

	/** Where the tear off push is applied, as a share of the half height above the door center (0 = center: the door slides upright; 1 = top edge: it tips over the most). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HingedDoor|TearOff", meta = (ClampMin = "0", ClampMax = "1"))
	float TearOffPushHeight = 0.8f;
};
