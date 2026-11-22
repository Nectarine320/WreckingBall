#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_LabelPopComponent.generated.h"

class UWidget;
class UWB_LabelPopProfile;

/**
 * Plays a squash and stretch on a widget (the P1-P4 label above a character) by driving its render scale from the curves of a UWB_LabelPopProfile.
 * Give it the widget once (SetTargetWidget), then call PlayPop each time a player takes the character. Ticks only while a pop plays.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_LabelPopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_LabelPopComponent();

	/** Settings of the pop: duration, scale curves, pivot (DA_LabelPop_Default). Required: empty, PlayPop does nothing and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LabelPop")
	TObjectPtr<UWB_LabelPopProfile> Profile;

	/** Sets the widget that pops (the label). Stops a pop playing on the previous widget and puts it back to scale 1. */
	UFUNCTION(BlueprintCallable, Category = "LabelPop")
	void SetTargetWidget(UWidget* Widget);

	/** Plays the pop from the start, even when one is already playing. Does nothing without a profile or a target widget. */
	UFUNCTION(BlueprintCallable, Category = "LabelPop")
	void PlayPop();

	/** Stops the pop and puts the widget back to scale 1. */
	UFUNCTION(BlueprintCallable, Category = "LabelPop")
	void StopPop();

	/** True while a pop plays. */
	UFUNCTION(BlueprintPure, Category = "LabelPop")
	bool IsPopPlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UWidget> TargetWidget;

	bool bPlaying = false;
	float Elapsed = 0.f;
	mutable bool bWarnedNoProfile = false;
};
