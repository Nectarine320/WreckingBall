#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "WB_SurfaceMovementComponent.generated.h"

class UWB_SurfaceMovementTable;
class UWB_SurfaceMovementProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnSurfaceChanged, TEnumAsByte<EPhysicalSurface>, NewSurface);

/**
 * Floor surfaces (lot 12.1): reads the Physical Surface under the player feet and, only when it changes, applies its profile from
 * DA_SurfaceMovementTable to the Character Movement (acceleration, braking, friction from the base values, never stacked) and to the
 * move speed (through UWB_PlayerStateComponent). Added to BP_GenericCharacter, so players and bots both feel the floors.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_SurfaceMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_SurfaceMovementComponent();

	/** Floor surfaces and their movement profiles (DA_SurfaceMovementTable). Required: empty, the floors change nothing and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceMovement")
	TObjectPtr<UWB_SurfaceMovementTable> SurfaceTable;

	/** Called when the player steps on another floor surface (after the new profile is applied). */
	UPROPERTY(BlueprintAssignable, Category = "SurfaceMovement")
	FWB_OnSurfaceChanged OnSurfaceChanged;

	/** Physical Surface currently under the feet (kept while in the air). */
	UFUNCTION(BlueprintPure, Category = "SurfaceMovement")
	TEnumAsByte<EPhysicalSurface> GetCurrentSurface() const { return CurrentSurface; }

	/** Profile applied now. Null: no profile for this floor (no change). */
	UFUNCTION(BlueprintPure, Category = "SurfaceMovement")
	const UWB_SurfaceMovementProfile* GetCurrentProfile() const { return CurrentProfile.Get(); }

	/** Dash distance multiplier of the current floor (1 without profile). For the Dash (lot 12.2). */
	UFUNCTION(BlueprintPure, Category = "SurfaceMovement")
	float GetDashDistanceMultiplier() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Character Movement values at BeginPlay: the multipliers always start from them. */
	float BaseMaxAcceleration = 0.f;
	float BaseBrakingDeceleration = 0.f;
	float BaseGroundFriction = 0.f;
	bool bHasBaseValues = false;

	TEnumAsByte<EPhysicalSurface> CurrentSurface = SurfaceType_Default;
	TWeakObjectPtr<const UWB_SurfaceMovementProfile> CurrentProfile;
	bool bSurfaceKnown = false;

	/** Physical Surface of the floor the Character Movement stands on. bOutOnGround is false in the air (the last surface is returned). */
	TEnumAsByte<EPhysicalSurface> ReadFloorSurface(bool& bOutOnGround) const;

	void ApplySurface(TEnumAsByte<EPhysicalSurface> Surface);
};
