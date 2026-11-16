#include "Feedback/WB_FeedbackLibrary.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Feedback/WB_FeedbackSettings.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool UWB_FeedbackSubsystem::TryConsumeCameraShake(float MinInterval)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (Now - LastCameraShakeTime < MinInterval)
	{
		return false;
	}
	LastCameraShakeTime = Now;
	return true;
}

UWB_FeedbackSettings* UWB_FeedbackLibrary::GetFeedbackSettings()
{
	const UWB_FeedbackDeveloperSettings* DevSettings = GetDefault<UWB_FeedbackDeveloperSettings>();
	return DevSettings ? DevSettings->FeedbackSettings.LoadSynchronous() : nullptr;
}

APawn* UWB_FeedbackLibrary::GetResponsiblePawn(AActor* Source)
{
	if (!IsValid(Source))
	{
		return nullptr;
	}
	if (APawn* Pawn = Cast<APawn>(Source))
	{
		return Pawn;
	}
	return Source->GetInstigator();
}

void UWB_FeedbackLibrary::SetResponsiblePawn(AActor* Target, APawn* Responsible)
{
	if (IsValid(Target))
	{
		Target->SetInstigator(Responsible);
	}
}

bool UWB_FeedbackLibrary::PlayFeedback(const UObject* WorldContextObject, FGameplayTag FeedbackTag, AActor* Source)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UWB_FeedbackSettings* Settings = GetFeedbackSettings();
	if (!World || !Settings || !FeedbackTag.IsValid())
	{
		return false;
	}
	const FWB_FeedbackEntry* Entry = Settings->Entries.Find(FeedbackTag);
	if (!Entry)
	{
		return false;
	}

	bool bPlayed = false;

	// Vibration: only the gamepad of the responsible player.
	if (Settings->bEnableHaptics && Entry->ForceFeedback && Settings->HapticsScale > 0.f)
	{
		if (const APawn* Pawn = GetResponsiblePawn(Source))
		{
			if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
			{
				PlayerController->ForceFeedbackScale = Settings->HapticsScale;
				FForceFeedbackParameters Params;
				Params.Tag = FeedbackTag.GetTagName(); // The same feedback restarts instead of stacking.
				PlayerController->ClientPlayForceFeedback(Entry->ForceFeedback, Params);
				bPlayed = true;
			}
		}
	}

	// Camera shake: shared camera, rate limited across the whole world.
	const float ShakeScale = Settings->CameraShakeScale * Entry->CameraShakeEntryScale;
	if (Settings->bEnableCameraShake && Entry->CameraShake && ShakeScale > 0.f)
	{
		UWB_FeedbackSubsystem* Subsystem = World->GetSubsystem<UWB_FeedbackSubsystem>();
		if (Subsystem && Subsystem->TryConsumeCameraShake(Settings->CameraShakeMinInterval))
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PlayerController = It->Get();
				if (PlayerController && PlayerController->IsLocalController() && PlayerController->PlayerCameraManager)
				{
					PlayerController->PlayerCameraManager->StartCameraShake(Entry->CameraShake, ShakeScale);
					bPlayed = true;
				}
			}
		}
	}

	return bPlayed;
}
