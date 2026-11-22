#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WB_SoccerCameraProfile.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Settings of the shared match camera (BP_SoccerCamera): ball follow, distance, angle, lag and field of view.
 * Asset: DA_SoccerCamera_Default, set on the CameraProfile variable of BP_SoccerCamera. The lot B camera (ball and carrier framing) extends it.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_SoccerCameraProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** How fast the camera catches up with the active ball (interpolation speed, per second). Higher = tighter follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0"))
	float BallFollowSpeed = 5.f;

	/** Distance in cm between the camera and the point it frames (spring arm length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "0", Units = "Centimeters"))
	float ArmLength = 3000.f;

	/** Downward angle of the camera in degrees (spring arm pitch, negative looks down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "-89", ClampMax = "0", Units = "Degrees"))
	float Pitch = -70.f;

	/** Horizontal field of view of the camera in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "5", ClampMax = "170", Units = "Degrees"))
	float FieldOfView = 55.f;

	/**
	 * True: the image keeps AspectRatio on every screen, with black bars on the sides (wider screen) or top and bottom (taller screen),
	 * so every player sees the same framing. The off-screen indicators stay inside the image. False: the image fills the window.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing")
	bool bConstrainAspectRatio = false;

	/** Width / height of the image when bConstrainAspectRatio is on. 1.7778 = 16:9. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "0.5", ClampMax = "4", EditCondition = "bConstrainAspectRatio"))
	float AspectRatio = 16.f / 9.f;

	/** True: the camera trails its target with a lag (on top of BallFollowSpeed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow")
	bool bEnableCameraLag = true;

	/** Lag speed of the spring arm when bEnableCameraLag is on. Higher = less lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", EditCondition = "bEnableCameraLag"))
	float CameraLagSpeed = 5.f;

	/**
	 * Ball held: share of the half screen width (0 = center, 1 = edge) between the screen center and the carrier, on the side opposite to where it goes.
	 * 0.667 puts the carrier in the middle of the left or right third. Used for the left and right moves.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Carrier", meta = (ClampMin = "0", ClampMax = "0.95"))
	float CarrierScreenOffsetHorizontal = 0.667f;

	/**
	 * Ball held: share of the half screen height (0 = center, 1 = edge) between the screen center and the carrier, on the side opposite to where it goes.
	 * 0.667 puts the carrier in the middle of the top or bottom third. Used for the up and down moves.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Carrier", meta = (ClampMin = "0", ClampMax = "0.95"))
	float CarrierScreenOffsetVertical = 0.667f;

	/** Ball held: the carrier's direction is updated only above this speed in cm/s. Slower (or standing), the camera keeps the last direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Carrier", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float CarrierDirectionMinSpeed = 50.f;

	/**
	 * Ball held: how fast the framing follows a change of the carrier's direction, per second. Lower = softer: a quick turn barely moves
	 * the camera, a held direction slowly takes over, and a U-turn passes through the center. 0: the framing follows the direction at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Carrier", meta = (ClampMin = "0"))
	float CarrierDirectionSmoothing = 3.f;

	/**
	 * Point the camera should frame this frame (call every tick, then interpolate toward it with BallFollowSpeed).
	 * Ball free: the ball. Ball held: the middle of the ball and its carrier, moved so it sits in the screen third opposite to where the carrier goes.
	 * CameraView: the camera component (gives the screen axes). LastCarrierDirection: the OutCarrierDirection of the previous frame (keep it in a variable).
	 * OutCarrierDirection: the smoothed direction (CarrierDirectionSmoothing), shorter than 1 while it turns; zero when the ball is free.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera", meta = (ReturnDisplayName = "Target"))
	FVector ComputeCameraTarget(const AActor* Ball, const USceneComponent* CameraView, FVector LastCarrierDirection, FVector& OutCarrierDirection) const;

	/**
	 * Applies the framing and lag settings of Profile to the spring arm and the camera (call from the Construction Script).
	 * Profile empty: a warning is logged and nothing is changed. Returns true when applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera", meta = (ReturnDisplayName = "Applied"))
	static bool ApplyCameraProfile(const UWB_SoccerCameraProfile* Profile, USpringArmComponent* SpringArm, UCameraComponent* Camera);
};
