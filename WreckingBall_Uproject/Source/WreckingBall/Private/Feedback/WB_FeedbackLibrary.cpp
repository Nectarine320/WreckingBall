#include "Feedback/WB_FeedbackLibrary.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
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

void UWB_FeedbackSubsystem::ClearResponsibleWhenAsleep(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsSimulatingPhysics())
		{
			// Sleep events are only sent when the body asks for wake events.
			Primitive->BodyInstance.bGenerateWakeEvents = true;
			Primitive->OnComponentSleep.AddUniqueDynamic(this, &UWB_FeedbackSubsystem::HandleComponentSleep);
		}
	}
}

void UWB_FeedbackSubsystem::HandleComponentSleep(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
	AActor* Owner = SleepingComponent ? SleepingComponent->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}
	// At rest: nobody threw it anymore. Thrown again later, SetResponsiblePawn marks it and watches it again.
	Owner->SetInstigator(nullptr);
	StopWatching(Owner);
}

void UWB_FeedbackSubsystem::StopWatching(AActor* Target)
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive)
		{
			Primitive->OnComponentSleep.RemoveDynamic(this, &UWB_FeedbackSubsystem::HandleComponentSleep);
		}
	}
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
		if (Responsible)
		{
			if (UWorld* World = Target->GetWorld())
			{
				if (UWB_FeedbackSubsystem* Subsystem = World->GetSubsystem<UWB_FeedbackSubsystem>())
				{
					Subsystem->ClearResponsibleWhenAsleep(Target);
				}
			}
		}
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
