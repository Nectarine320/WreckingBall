#include "Camera/WB_CameraModeComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Camera/WB_CameraTags.h"
#include "Camera/WB_SoccerCameraProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Gym/WB_GymDirectorComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBCameraMode, Log, All);

namespace
{
	bool IsKnownMode(const FGameplayTag& Mode)
	{
		return Mode.MatchesTagExact(WBTags::Camera_Mode_Ball) || Mode.MatchesTagExact(WBTags::Camera_Mode_Player1);
	}

	// Temporary debug command (devis gym-camera-p1, lot 4): remove it once the debug menu calls SetCameraMode.
	FAutoConsoleCommandWithWorldAndArgs GCameraModeCommand(
		TEXT("WB.CameraMode"),
		TEXT("Changes the shared camera mode: WB.CameraMode Ball | Player1 (temporary, until the debug menu)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWB_CameraModeComponent* Modes = UWB_CameraModeComponent::GetCameraModeComponent(World);
			if (!Modes)
			{
				UE_LOG(LogWBCameraMode, Warning, TEXT("WB.CameraMode: the camera manager of Player 1 has no UWB_CameraModeComponent."));
				return;
			}
			const FString Name = Args.Num() > 0 ? Args[0] : FString();
			if (Name.Equals(TEXT("Ball"), ESearchCase::IgnoreCase))
			{
				Modes->SetCameraMode(WBTags::Camera_Mode_Ball);
			}
			else if (Name.Equals(TEXT("Player1"), ESearchCase::IgnoreCase))
			{
				Modes->SetCameraMode(WBTags::Camera_Mode_Player1);
			}
			else
			{
				UE_LOG(LogWBCameraMode, Warning, TEXT("WB.CameraMode: Ball or Player1 expected. Current mode: %s."), *Modes->GetCameraMode().ToString());
			}
		}));
}

UWB_CameraModeComponent::UWB_CameraModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWB_CameraModeComponent* UWB_CameraModeComponent::GetCameraModeComponent(const UObject* WorldContextObject)
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(WorldContextObject, 0);
	return CameraManager ? CameraManager->FindComponentByClass<UWB_CameraModeComponent>() : nullptr;
}

FGameplayTag UWB_CameraModeComponent::GetCameraMode() const
{
	return CurrentMode.IsValid() ? CurrentMode : WBTags::Camera_Mode_Ball.GetTag();
}

void UWB_CameraModeComponent::SetCameraMode(FGameplayTag ModeTag)
{
	if (!IsKnownMode(ModeTag))
	{
		UE_LOG(LogWBCameraMode, Warning, TEXT("SetCameraMode: %s is not a camera mode (Camera.Mode.Ball or Camera.Mode.Player1), mode unchanged."), *ModeTag.ToString());
		return;
	}
	const FGameplayTag OldMode = GetCameraMode();
	CurrentMode = ModeTag;
	if (OldMode != ModeTag)
	{
		UE_LOG(LogWBCameraMode, Log, TEXT("Camera mode: %s."), *ModeTag.ToString());
		OnCameraModeChanged.Broadcast(OldMode, ModeTag);
	}
}

void UWB_CameraModeComponent::ApplyStartMode(FGameplayTag GameModeDefault)
{
	FGameplayTag Mode;
	if (const UWB_GymDirectorComponent* Gym = UWB_GymDirectorComponent::GetGymDirector(this))
	{
		Mode = Gym->GetDefaultCameraMode();
	}
	if (!IsKnownMode(Mode))
	{
		Mode = GameModeDefault;
	}
	if (!IsKnownMode(Mode))
	{
		Mode = WBTags::Camera_Mode_Ball;
	}
	SetCameraMode(Mode);
}

bool UWB_CameraModeComponent::ComputeCameraTarget(const UWB_SoccerCameraProfile* Profile, const AActor* Ball, const USceneComponent* CameraView, FVector LastDirection,
	FVector& OutTarget, FVector& OutDirection) const
{
	OutTarget = FVector::ZeroVector;
	OutDirection = FVector::ZeroVector;
	if (!Profile)
	{
		return false;
	}
	if (GetCameraMode().MatchesTagExact(WBTags::Camera_Mode_Player1))
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Player)
		{
			return false;
		}
		OutTarget = Profile->ComputePlayerCameraTarget(Player, CameraView, LastDirection, OutDirection);
		return true;
	}
	// A ball being respawned has its collision off: the camera waits for it.
	if (!Ball || !Ball->GetActorEnableCollision())
	{
		return false;
	}
	OutTarget = Profile->ComputeCameraTarget(Ball, CameraView, LastDirection, OutDirection);
	return true;
}
