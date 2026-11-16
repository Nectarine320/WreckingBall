#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "WB_FeedbackLibrary.generated.h"

class APawn;
class UWB_FeedbackSettings;

/** Keeps the state shared by every feedback request of a world (camera shake rate limit). */
UCLASS()
class WRECKINGBALL_API UWB_FeedbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** True if a camera shake may play now (at least MinInterval seconds since the last one); records it when true. */
	bool TryConsumeCameraShake(float MinInterval);

private:
	double LastCameraShakeTime = -1.0e9;
};

/**
 * Player feedback (controller vibration, camera shake), driven by the Feedback.* Gameplay Tags of DA_FeedbackSettings.
 */
UCLASS()
class WRECKINGBALL_API UWB_FeedbackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Plays the catalog entry of FeedbackTag: vibration on the gamepad of the player responsible for Source, camera shake on the shared camera.
	 * Source: the pawn itself (Dash) or the object that hit (its instigator is the last player who threw or kicked it). No responsible player: no vibration.
	 * Returns true when something played.
	 */
	UFUNCTION(BlueprintCallable, Category = "Feedback", meta = (WorldContext = "WorldContextObject", Categories = "Feedback"))
	static bool PlayFeedback(const UObject* WorldContextObject, FGameplayTag FeedbackTag, AActor* Source);

	/** Returns the player responsible for Source: Source itself when it is a pawn, else its instigator (last player who threw or kicked it). None if there is none. */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	static APawn* GetResponsiblePawn(AActor* Source);

	/** Marks Responsible as the player responsible for Target (sets its instigator). Call it when a player throws, kicks or dashes into an object. Responsible defaults to self in a pawn. */
	UFUNCTION(BlueprintCallable, Category = "Feedback", meta = (DefaultToSelf = "Responsible"))
	static void SetResponsiblePawn(AActor* Target, APawn* Responsible);

	/** Returns the player feedback catalog set in Project Settings > Game > Wrecking Ball Feedback. None if unset. */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	static UWB_FeedbackSettings* GetFeedbackSettings();
};
