#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WB_GroundRingComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UWB_GroundRingProfile;

/**
 * Procedural ring drawn on the ground with a decal (UI 3D devis, lot A): team ring and kick gauge under a player, marker under the active ball.
 * Creates its UDecalComponent and the dynamic material once, at BeginPlay. The decal always projects straight down, whatever the owner's rotation.
 * Place the component at the feet (player). With the profile's bSnapToGround, the decal follows the ground under the owner instead (ball).
 * Drives the M_GroundRing parameters RingColor, KickPercent and ShadowStrength.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_GroundRingComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWB_GroundRingComponent();

	/** Settings of the ring (DA_GroundRing_Player, DA_GroundRing_Ball). Required: empty, no ring is created and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing")
	TObjectPtr<UWB_GroundRingProfile> Profile;

	/** Sets the outline color (team color). Kept and applied at BeginPlay when called earlier. */
	UFUNCTION(BlueprintCallable, Category = "GroundRing")
	void SetRingColor(FLinearColor Color);

	/** Sets the kick gauge: 0 = plain ring, 1 = full arc around the player. Clamped 0..1. */
	UFUNCTION(BlueprintCallable, Category = "GroundRing")
	void SetKickPercent(float Percent);

	/** Shows or hides the ring. While hidden, a snapped ring does not trace. */
	UFUNCTION(BlueprintCallable, Category = "GroundRing")
	void SetRingActive(bool bActive);

	/** True while the ring is shown (it may still be hidden by a snapped ring that found no ground). */
	UFUNCTION(BlueprintPure, Category = "GroundRing")
	bool IsRingActive() const { return bRingActive; }

	/** The decal created at BeginPlay. None before BeginPlay or without a profile. */
	UFUNCTION(BlueprintPure, Category = "GroundRing")
	UDecalComponent* GetDecal() const { return Decal; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	bool bRingActive = false;
	bool bHasRingColor = false;
	FLinearColor RingColor = FLinearColor::White;
	float KickPercent = 0.f;

	/** ShadowStrength of the material instance, read when the dynamic material is created (base of the height fade). */
	float BaseShadowStrength = 0.f;

	bool UsesSnap() const;

	/** Shows the decal when active (and on the ground for a snapped ring), and turns the tick on only while snapping. */
	void RefreshState();

	/** Snapped ring: places the decal on the ground under the owner. Returns false when no ground was found. */
	bool UpdateSnap();
};
