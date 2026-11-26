#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_XRayOutlineComponent.generated.h"

class UPrimitiveComponent;
class UWB_XRayOutlineProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnXRayOcclusionChanged, bool, bOccluded);

/**
 * Shows the owner's silhouette through walls only while it is hidden from the shared camera (2026-11-26).
 * Every CheckInterval, traces from the camera of player 0 to the center of the owner and to a ring of edge points, on the profile's channel
 * (XRayOcclusion). When enough points are blocked, Render Custom Depth is turned on for the target meshes, with the profile's stencil value;
 * the map's Post Process Volume draws the silhouette. Render Custom Depth changes only when the occlusion state changes.
 * Targets: every mesh component of the owner, unless SetTargetComponents gives a list.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_XRayOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_XRayOutlineComponent();

	/** X-ray settings (DA_XRayOutline_Ball for the balls). Empty: the component logs a warning and does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline")
	TObjectPtr<UWB_XRayOutlineProfile> Profile;

	/** Starts the checks on BeginPlay. Off: call SetXRayEnabled(true) when the situation needs it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline")
	bool bEnabledOnBeginPlay = true;

	/** Draws each trace for one check: green when free, red when blocked. Debug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDrawTraces = false;

	/** Fires when the silhouette turns on (true) or off (false). */
	UPROPERTY(BlueprintAssignable, Category = "XRayOutline")
	FWB_OnXRayOcclusionChanged OnOcclusionChanged;

	/** Starts or stops the checks. Stopping also hides the silhouette. */
	UFUNCTION(BlueprintCallable, Category = "XRayOutline")
	void SetXRayEnabled(bool bEnabled);

	/** True while the checks run. */
	UFUNCTION(BlueprintPure, Category = "XRayOutline")
	bool IsXRayEnabled() const { return bXRayEnabled; }

	/** True while the silhouette is shown (the owner is hidden from the camera). */
	UFUNCTION(BlueprintPure, Category = "XRayOutline")
	bool IsOccluded() const { return bOccluded; }

	/** Replaces the meshes that get the silhouette (and give the traced center). Empty: back to every mesh component of the owner. */
	UFUNCTION(BlueprintCallable, Category = "XRayOutline")
	void SetTargetComponents(const TArray<UPrimitiveComponent*>& NewTargets);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CollectDefaultTargets();
	void CheckOcclusion();
	bool ComputeOccluded(bool& bOutValid) const;
	void ApplyOccluded(bool bNewOccluded);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Targets;

	FTimerHandle CheckTimer;
	bool bXRayEnabled = false;
	bool bOccluded = false;
	double LastStateChangeTime = -1.0e9;
};
