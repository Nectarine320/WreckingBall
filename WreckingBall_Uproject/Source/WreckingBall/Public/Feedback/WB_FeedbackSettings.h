#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "WB_FeedbackSettings.generated.h"

class UCameraShakeBase;
class UForceFeedbackEffect;

/**
 * One player feedback of the catalog, played by UWB_FeedbackLibrary::PlayFeedback through its Gameplay Tag.
 * Each part is optional: an empty field plays nothing.
 */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_FeedbackEntry
{
	GENERATED_BODY()

	/** Controller vibration, played only on the gamepad of the responsible player. Empty: no vibration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|Haptics")
	TObjectPtr<UForceFeedbackEffect> ForceFeedback = nullptr;

	/** Camera shake, played on the shared camera (every player sees it). Empty: no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|CameraShake")
	TSubclassOf<UCameraShakeBase> CameraShake;

	/** Scale of this entry's camera shake, multiplied by the global CameraShakeScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|CameraShake", meta = (ClampMin = "0"))
	float CameraShakeEntryScale = 1.f;
};

/**
 * Player feedback catalog (vibration, camera shake) with its global switches.
 * Gameplay code asks for a feedback by Gameplay Tag (Feedback.*); the matching entry says what plays.
 * The project uses the asset set in Project Settings > Game > Wrecking Ball Feedback.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_FeedbackSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Global switch for controller vibration. Off: no entry vibrates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|Haptics")
	bool bEnableHaptics = true;

	/** Global vibration strength (0 = none, 1 = as authored in the effects). Applied to the responsible player's controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|Haptics", meta = (ClampMin = "0", ClampMax = "2", EditCondition = "bEnableHaptics"))
	float HapticsScale = 1.f;

	/** Global switch for camera shakes. Off: no entry shakes the camera (one checkbox to remove them if too many things break). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|CameraShake")
	bool bEnableCameraShake = true;

	/** Global camera shake strength, multiplied by each entry's scale. Keep it low: the camera is shared by every player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|CameraShake", meta = (ClampMin = "0", EditCondition = "bEnableCameraShake"))
	float CameraShakeScale = 0.5f;

	/** Minimum time in seconds between two camera shakes. Shakes asked sooner are skipped, so several walls falling together do not add up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|CameraShake", meta = (ClampMin = "0", Units = "Seconds", EditCondition = "bEnableCameraShake"))
	float CameraShakeMinInterval = 0.3f;

	/** The catalog: what each Feedback.* tag plays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback", meta = (Categories = "Feedback"))
	TMap<FGameplayTag, FWB_FeedbackEntry> Entries;
};

/** Project setting that points to the player feedback catalog used in game. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wrecking Ball Feedback"))
class WRECKINGBALL_API UWB_FeedbackDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Player feedback catalog (DA_FeedbackSettings) used by PlayFeedback. Empty: PlayFeedback does nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	TSoftObjectPtr<UWB_FeedbackSettings> FeedbackSettings;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
