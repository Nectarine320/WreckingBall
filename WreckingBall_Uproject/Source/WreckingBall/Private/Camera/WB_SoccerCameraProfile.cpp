#include "Camera/WB_SoccerCameraProfile.h"

#include "Ball/WB_BallPossessionSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBSoccerCamera, Log, All);

bool UWB_SoccerCameraProfile::ApplyCameraProfile(const UWB_SoccerCameraProfile* Profile, USpringArmComponent* SpringArm, UCameraComponent* Camera)
{
	if (!Profile)
	{
		UE_LOG(LogWBSoccerCamera, Warning, TEXT("ApplyCameraProfile: no camera profile (DA_SoccerCamera_Default expected on BP_SoccerCamera). Camera settings left unchanged."));
		return false;
	}
	if (SpringArm)
	{
		SpringArm->TargetArmLength = Profile->ArmLength;
		const FRotator Rotation = SpringArm->GetRelativeRotation();
		SpringArm->SetRelativeRotation(FRotator(Profile->Pitch, Rotation.Yaw, Rotation.Roll));
		SpringArm->bEnableCameraLag = Profile->bEnableCameraLag;
		SpringArm->CameraLagSpeed = Profile->CameraLagSpeed;
	}
	if (Camera)
	{
		Camera->SetFieldOfView(Profile->FieldOfView);
		Camera->SetConstraintAspectRatio(Profile->bConstrainAspectRatio);
		Camera->SetAspectRatio(Profile->AspectRatio);
	}
	return true;
}

FVector UWB_SoccerCameraProfile::ComputeCameraTarget(const AActor* Ball, const USceneComponent* CameraView, FVector LastCarrierDirection, FVector& OutCarrierDirection) const
{
	OutCarrierDirection = FVector::ZeroVector;
	if (!Ball)
	{
		return FVector::ZeroVector;
	}
	const FVector BallLocation = Ball->GetActorLocation();
	const AActor* Carrier = UWB_BallPossessionSubsystem::GetBallPossessor(Ball, Ball);
	if (!Carrier || !CameraView)
	{
		return BallLocation;
	}
	return ComputeWalkFraming((BallLocation + Carrier->GetActorLocation()) * 0.5f, Carrier, CameraView, LastCarrierDirection, OutCarrierDirection);
}

FVector UWB_SoccerCameraProfile::ComputePlayerCameraTarget(const APawn* Player, const USceneComponent* CameraView, FVector LastDirection, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;
	if (!Player)
	{
		return FVector::ZeroVector;
	}
	if (!CameraView)
	{
		return Player->GetActorLocation();
	}
	return ComputeWalkFraming(Player->GetActorLocation(), Player, CameraView, LastDirection, OutDirection);
}

FVector UWB_SoccerCameraProfile::ComputeWalkFraming(const FVector& Anchor, const AActor* Mover, const USceneComponent* CameraView, FVector LastDirection, FVector& OutDirection) const
{
	// Direction the mover goes: its flat velocity, or the last one while it stands still.
	FVector Velocity = Mover->GetVelocity();
	Velocity.Z = 0.f;
	LastDirection.Z = 0.f;
	const FVector WantedDirection = Velocity.Size() >= CarrierDirectionMinSpeed ? Velocity.GetSafeNormal() : LastDirection;
	// The direction itself is eased (not normalized): a U-turn shrinks it through zero, so the framing passes through the center.
	const UWorld* World = Mover->GetWorld();
	OutDirection = CarrierDirectionSmoothing > 0.f && World
		? FMath::VInterpTo(LastDirection, WantedDirection, World->GetDeltaSeconds(), CarrierDirectionSmoothing)
		: WantedDirection;
	if (OutDirection.IsNearlyZero())
	{
		return Anchor;
	}
	const FVector OutCarrierDirection = OutDirection;

	// Screen axes on the ground: right and up of the screen.
	const FRotator Yaw(0.f, CameraView->GetComponentRotation().Yaw, 0.f);
	const FVector ScreenRight = Yaw.RotateVector(FVector::RightVector);
	const FVector ScreenUp = Yaw.RotateVector(FVector::ForwardVector);
	const float SideAmount = FVector::DotProduct(OutCarrierDirection, ScreenRight);
	const float UpAmount = FVector::DotProduct(OutCarrierDirection, ScreenUp);

	// Image aspect (width / height): the constrained one, else the viewport's (16:9 before the viewport exists, technical value).
	float Aspect = bConstrainAspectRatio ? AspectRatio : 16.f / 9.f;
	if (!bConstrainAspectRatio && GEngine && GEngine->GameViewport)
	{
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			Aspect = Size.X / Size.Y;
		}
	}
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float TanHalfV = TanHalfH / Aspect;
	// Angle between the view and the ground, kept away from 0 and 90 degrees (division guard). Technical value.
	const float Down = FMath::DegreesToRadians(FMath::Clamp(-Pitch, 5.f, 89.f));

	// Sideways: a share f of the half width maps to ArmLength x tan(halfFOV) x f on the ground.
	const float SideShift = ArmLength * TanHalfH * CarrierScreenOffsetHorizontal * SideAmount;

	// Up / down: a share f of the half height is a ray tilted by a = atan(f x tan(halfVFOV)).
	// Going up, the carrier sits below the center (nearer the camera): ground gap = L sin(a) / sin(Down + a).
	// Going down, it sits above (farther): ground gap = L sin(a) / sin(Down - a).
	const float A = FMath::Atan(CarrierScreenOffsetVertical * FMath::Abs(UpAmount) * TanHalfV);
	const float Denominator = UpAmount >= 0.f ? FMath::Sin(Down + A) : FMath::Sin(FMath::Max(Down - A, 0.05f));
	const float UpShift = FMath::Sign(UpAmount) * ArmLength * FMath::Sin(A) / Denominator;

	return Anchor + ScreenRight * SideShift + ScreenUp * UpShift;
}
