#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_BallDribbleComponent.generated.h"

class ACharacter;
class UPrimitiveComponent;
class UWB_BallDribbleProfile;
class UWB_PlayerStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnDribbleLost, AActor*, Ball);

/**
 * Dribbles the possessed ball on the ground (dribble devis 2026-11-19): replaces the Physics Handle for balls.
 * The ball stays fully simulated (gravity, floor, bounces, collisions). Each frame, before physics, a flat push (X and Y only) drives it toward a point
 * just in front of the feet, around the capsule on sharp turns, and its spin follows the roll. Too far from that point (LoseDistance for LoseDelay)
 * or too high (LoseHeight): the possession is lost and OnDribbleLost fires. Options of the profile, off by default: elastic tether, ball touches,
 * locked possession (bLockPossession: the ball velocity is set each frame, never lost by itself, put back at the feet when stuck).
 * BPC_GrabObject calls StartDribble on pickup and StopDribble before a kick or a drop. Stops by itself when the player no longer holds anything.
 * Records the dribbler in UWB_BallPossessionSubsystem, so IsBallHeld stays true while the ball is dribbled.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_BallDribbleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_BallDribbleComponent();

	/** Dribble settings used when the ball's PDA_PhysicsProfile gives none (DA_BallDribble_Default). Both empty: StartDribble refuses and logs a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallDribble")
	TObjectPtr<UWB_BallDribbleProfile> DefaultProfile;

	/** Logs why each dribble is lost (height or distance), with the values. Debug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugLogLoss = false;

	/** Draws the dribble point, the push and the loss radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDrawDribble = false;

	/** Fires after the possession was lost by distance or height (the dribble is already stopped). Not fired by StopDribble. */
	UPROPERTY(BlueprintAssignable, Category = "BallDribble")
	FWB_OnDribbleLost OnDribbleLost;

	/**
	 * Starts dribbling Ball. BallProfile: the DribbleProfile of the ball's PDA_PhysicsProfile (may be empty: DefaultProfile is used).
	 * Returns false, without dribbling, when no profile is set, the ball has no simulated body, or another player already possesses it.
	 */
	UFUNCTION(BlueprintCallable, Category = "BallDribble")
	bool StartDribble(AActor* Ball, UWB_BallDribbleProfile* BallProfile);

	/** Stops dribbling and frees the ball, without any impulse (the kick or the drop adds its own after). Safe when not dribbling. */
	UFUNCTION(BlueprintCallable, Category = "BallDribble")
	void StopDribble();

	/** True between StartDribble and StopDribble (or the loss). */
	UFUNCTION(BlueprintPure, Category = "BallDribble")
	bool IsDribbling() const { return DribbledBall.IsValid(); }

	/** Ball being dribbled, or none. */
	UFUNCTION(BlueprintPure, Category = "BallDribble")
	AActor* GetDribbledBall() const { return DribbledBall.Get(); }

	/** Simulated body of the dribbled ball (the component to kick), or none. */
	UFUNCTION(BlueprintPure, Category = "BallDribble")
	UPrimitiveComponent* GetDribbledBody() const { return DribbledBody.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AActor> DribbledBall;
	TWeakObjectPtr<UPrimitiveComponent> DribbledBody;

	UPROPERTY(Transient)
	TObjectPtr<UWB_BallDribbleProfile> ActiveProfile;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UWB_PlayerStateComponent> PlayerState;

	/** Seconds the ball has spent beyond LoseDistance. */
	float TimeBeyondLoseDistance = 0.f;

	/** World time of StartDribble (CatchHeightGrace). */
	double DribbleStartTime = 0.0;

	/** World time of the last ball touch (touch option). */
	double LastTouchTime = -1000.0;

	/** True when the bottom of the ball is within GroundTolerance of the floor. */
	bool IsBallOnGround(const UPrimitiveComponent* Body, float BallRadius) const;

	/** Flat point the ball heads to: Target, or a point beside the capsule when the straight way crosses it. */
	FVector GetPathPoint(const FVector& BallCenter, const FVector& Target, const FVector& PlayerCenter, float KeepOutRadius, const FVector& Forward) const;

	/** Locked possession: puts the ball back at the dribble point (swept from the player, so it never ends inside a wall), at the player's velocity. */
	void PutBallAtFeet(UPrimitiveComponent* Body, const FVector& DribblePoint, const FVector& PlayerCenter, float FeetZ, float BallRadius, const FVector& PlayerFlatVelocity);

	/** Stops and broadcasts OnDribbleLost. */
	void LoseBall(const TCHAR* Reason = TEXT(""));
};
