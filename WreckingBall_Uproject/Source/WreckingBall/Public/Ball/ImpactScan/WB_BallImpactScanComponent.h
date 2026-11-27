#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "WB_BallImpactScanComponent.generated.h"

class UWB_BallImpactScanProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnBallScanHit, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWB_OnBallScanStopped);

/**
 * Impact scan of a kicked ball (replaces the KickScan timer of BP_PO_Master_Ball_01). StartScan, called on a kick, sweeps a sphere ahead
 * of the ball every ScanInterval while it goes faster than MinSpeed, and fires OnScanHit for each blocking hit. The scan stops by itself
 * when the ball slows down (OnScanStopped). The impact itself stays in the ball Blueprint (BPI_ImpactReceiver).
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_BallImpactScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_BallImpactScanComponent();

	/** Scan settings (DA_BallImpactScan_Default). Required: empty, StartScan does nothing and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallImpactScan")
	TObjectPtr<UWB_BallImpactScanProfile> Profile;

	/** Called for each blocking hit of a scan, ahead of the ball. Several hits per scan are possible. */
	UPROPERTY(BlueprintAssignable, Category = "BallImpactScan")
	FWB_OnBallScanHit OnScanHit;

	/** Called once when the scan stops because the ball went under MinSpeed. */
	UPROPERTY(BlueprintAssignable, Category = "BallImpactScan")
	FWB_OnBallScanStopped OnScanStopped;

	/** Starts (or keeps running) the scan. Call it right after a kick. */
	UFUNCTION(BlueprintCallable, Category = "BallImpactScan")
	void StartScan();

	/** Stops the scan without firing OnScanStopped. */
	UFUNCTION(BlueprintCallable, Category = "BallImpactScan")
	void StopScan();

	/** True while the scan runs. */
	UFUNCTION(BlueprintPure, Category = "BallImpactScan")
	bool IsScanning() const;

	/** MinSpeed of the profile (cm/s), or 0 without profile. Lets the Blueprint use the same threshold for its own hit events. */
	UFUNCTION(BlueprintPure, Category = "BallImpactScan")
	float GetMinSpeed() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle ScanTimer;
	bool bWarnedNoProfile = false;

	void RunScan();
};
