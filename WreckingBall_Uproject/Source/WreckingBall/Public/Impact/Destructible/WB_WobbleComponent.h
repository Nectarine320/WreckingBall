#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Impact/Destructible/WB_WobbleTypes.h"
#include "WB_WobbleComponent.generated.h"

class USceneComponent;

/**
 * Wobbles a pivot (scene component) after a hit, then brings it back exactly to its rest rotation.
 * A new hit replaces the wobble in progress, starting from the current pose (no visual jump): the old pose fades out
 * over InterruptFadeTime while the new wobble starts. Works across pivots (wall side A, then side B).
 * Single axis mode (wall): rolls the pivot around its parent X axis, away from the hit side (same as the lot 9 wall).
 * Four faces mode (column): the hit normal, in the pivot parent space, is snapped to the dominant face (+X, -X, +Y, -Y)
 * and the pivot leans away from it.
 */
UCLASS(ClassGroup = (WreckingBall), Blueprintable, meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_WobbleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_WobbleComponent();

	/** False: single axis (wall), roll around the pivot parent X axis. True: four faces (column), lean away from the dominant hit face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble")
	bool bUseFourFaces = false;

	/** Four faces mode only: flips the lean direction, in case the impact normal convention points the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (EditCondition = "bUseFourFaces"))
	bool bInvertFourFacesDirection = false;

	/** Single axis mode only: minimum |dot| between the hit normal and the pivot parent Y axis for the hit to count as a side hit. Edge hits below it do not wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (EditCondition = "!bUseFourFaces", ClampMin = "0", ClampMax = "1"))
	float SingleAxisMinSideDot = 0.5f;

	/** Pivot that wobbles for a hit on the owner's +Y side (wall side A). Four faces mode: the only pivot (column base). Pick a scene component of the owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wobble", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	FComponentReference PositiveSidePivot;

	/** Single axis mode only: pivot that wobbles for a hit on the owner's -Y side (wall side B). Leave empty to use PositiveSidePivot for both sides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wobble", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent", EditCondition = "!bUseFourFaces"))
	FComponentReference NegativeSidePivot;

	/**
	 * Starts a wobble from a hit, on the pivot picked from the hit side (PositiveSidePivot / NegativeSidePivot).
	 * ImpactNormal is in world space (impact data Normal). Same rules and return value as StartWobbleFromImpact.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wobble")
	bool StartWobbleFromNormal(FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings);

	/**
	 * Starts a small wobble for a hit the destructible refused (damage below its MinDamagePerHit). Does nothing unless Settings.bWobbleOnMinorHits is on and Damage is above 0.
	 * Same pivot and side rules as StartWobbleFromNormal; the wobble is scaled down by the Minor Hits settings. Returns true when a wobble starts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wobble")
	bool StartMinorHitWobbleFromNormal(FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings);

	/** Returns the pivot StartWobbleFromNormal would use for this hit normal (world space). None if no pivot is set. */
	UFUNCTION(BlueprintPure, Category = "Wobble")
	USceneComponent* GetPivotForNormal(FVector ImpactNormal) const;

	/**
	 * Starts a wobble on Pivot from a hit. ImpactNormal is in world space (impact data Normal); Damage picks the Light/Strong blend.
	 * Interrupts any wobble in progress without a jump. Returns false when nothing starts (no pivot, wobble disabled in Settings, edge hit).
	 */
	UFUNCTION(BlueprintCallable, Category = "Wobble")
	bool StartWobbleFromImpact(USceneComponent* Pivot, FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings);

	/** Stops every wobble at once and puts each pivot back to its rest rotation. */
	UFUNCTION(BlueprintCallable, Category = "Wobble")
	void StopAllWobbles();

	/** True while at least one pivot is wobbling or fading out. */
	UFUNCTION(BlueprintPure, Category = "Wobble")
	bool IsWobbling() const { return States.Num() > 0; }

	/** True when the settings can produce a wobble (AngleLight and DurationLight above 0). */
	UFUNCTION(BlueprintPure, Category = "Wobble")
	static bool IsWobbleEnabled(const FWB_WobbleSettings& Settings);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FWB_PivotWobble
	{
		TWeakObjectPtr<USceneComponent> Pivot;
		FQuat Rest = FQuat::Identity;

		// Pose of an interrupted wobble, fading back to identity.
		FQuat FadeFrom = FQuat::Identity;
		float FadeTime = 0.f;
		float FadeElapsed = 0.f;

		// Current oscillation, around Axis (pivot parent space).
		bool bOscillating = false;
		FVector Axis = FVector::XAxisVector;
		float AmplitudeRad = 0.f;
		float Duration = 0.f;
		float Frequency = 0.f;
		float Elapsed = 0.f;
	};

	TArray<FWB_PivotWobble> States;

	/** Offset applied on top of the rest rotation, in the pivot parent space. */
	static FQuat ComputeOffset(const FWB_PivotWobble& State);
	static bool IsFinished(const FWB_PivotWobble& State);

	/** Fills Axis for the hit. Returns false for an edge hit in single axis mode. */
	bool ComputeTiltAxis(const USceneComponent* Pivot, const FVector& ImpactNormal, FVector& OutAxis) const;
};
