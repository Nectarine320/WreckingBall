#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WB_CameraModeComponent.generated.h"

class UWB_SoccerCameraProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnCameraModeChanged, FGameplayTag, OldMode, FGameplayTag, NewMode);

/**
 * Mode of the shared camera (devis gym-camera-p1, lot 4). Added to the camera manager CAMM_SoccerPlayer; BP_SoccerCamera asks it every tick
 * which point to frame (ComputeCameraTarget). Camera.Mode.Ball: the ball and its carrier (match, default). Camera.Mode.Player1: only the character
 * of Player 1, with the same walk-direction offset, also in a map without a ball. Both read the same DA_SoccerCamera_Default.
 * The switch is smoothed by the camera follow (BallFollowSpeed). Temporary debug: console command WB.CameraMode Ball | Player1 (until the debug menu).
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_CameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_CameraModeComponent();

	/** Called when the camera mode changes. */
	UPROPERTY(BlueprintAssignable, Category = "Camera|Mode")
	FWB_OnCameraModeChanged OnCameraModeChanged;

	/** Changes the camera mode (Camera.Mode.Ball or Camera.Mode.Player1). Another tag is refused (warning). The camera glides to the new target. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Mode")
	void SetCameraMode(FGameplayTag ModeTag);

	/** Current camera mode. Camera.Mode.Ball until a mode is set. */
	UFUNCTION(BlueprintPure, Category = "Camera|Mode")
	FGameplayTag GetCameraMode() const;

	/**
	 * Sets the start mode of the map, by priority: the debug console variable WB.StartWithPlayer1Camera (WB Tools > Gym Mode, not in shipping), the DefaultCameraMode of the active gym (GM_Gym), else GameModeDefault
	 * (DefaultCameraMode of BP_SoccerGamemode), else Camera.Mode.Ball. Call once from BP_SoccerCamera BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Mode")
	void ApplyStartMode(FGameplayTag GameModeDefault);

	/**
	 * Point to frame this frame for the current mode (interpolate toward it with BallFollowSpeed). Ball: Profile->ComputeCameraTarget on Ball
	 * (only while it has collision: a ball being respawned is skipped). Player1: Profile->ComputePlayerCameraTarget on the pawn of Player 1.
	 * Returns false when there is nothing to follow (no ball, no character of Player 1, no profile): keep the camera where it is.
	 * LastDirection / OutDirection: the walk direction of the previous frame / of this frame (keep it in a variable).
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Mode", meta = (ReturnDisplayName = "Has Target"))
	bool ComputeCameraTarget(const UWB_SoccerCameraProfile* Profile, const AActor* Ball, const USceneComponent* CameraView, FVector LastDirection,
		FVector& OutTarget, FVector& OutDirection) const;

	/** Camera mode component of the camera manager of Player 1. None when it has none. */
	UFUNCTION(BlueprintPure, Category = "Camera|Mode", meta = (WorldContext = "WorldContextObject"))
	static UWB_CameraModeComponent* GetCameraModeComponent(const UObject* WorldContextObject);

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Camera|Mode")
	FGameplayTag CurrentMode;
};
