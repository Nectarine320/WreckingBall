#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WB_GroundRingComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UWB_GroundRingProfile;

/**
 * Procedural ring drawn on the ground (UI 3D devis, lot A): team ring and kick gauge under a player, marker under the active ball.
 * Creates a UDecalComponent, or a flat UStaticMeshComponent with the profile's bUseMesh (no TSR ghosting), and the dynamic material once, at BeginPlay.
 * The ring always faces straight down, whatever the owner's rotation.
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

	/** The decal created at BeginPlay. None before BeginPlay, without a profile, or in mesh mode (bUseMesh). */
	UFUNCTION(BlueprintPure, Category = "GroundRing")
	UDecalComponent* GetDecal() const { return Decal; }

	/** The flat mesh created at BeginPlay in mesh mode (bUseMesh). None otherwise. */
	UFUNCTION(BlueprintPure, Category = "GroundRing")
	UStaticMeshComponent* GetRingMesh() const { return RingMesh; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RingMesh;

	/** The decal or the mesh, whichever was created: what is moved and shown. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> RingVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	bool bRingActive = false;
	bool bHasRingColor = false;
	FLinearColor RingColor = FLinearColor::White;
	float KickPercent = 0.f;

	/** ShadowStrength of the material instance, read when the dynamic material is created (base of the height fade). */
	float BaseShadowStrength = 0.f;

	/** Gauge thickness and colors of the material instance, read when the dynamic material is created (base of the full charge pulse). */
	float BaseGaugeThickness = 0.f;
	FLinearColor BaseKickColorStart = FLinearColor::White;
	FLinearColor BaseKickColorEnd = FLinearColor::White;

	/** Full charge pulse in progress (Profile->bUseFullChargePulse) and its elapsed time. */
	bool bPulsing = false;
	float PulseElapsed = 0.f;

	/** Applies the pulse at Weight (0: base look, 1: peak). */
	void ApplyPulse(float Weight);

	bool UsesSnap() const;

	/** Creates the decal (default) or the flat mesh (bUseMesh). Returns false and logs a warning when the profile misses an asset. */
	bool CreateDecal(AActor* Owner);
	bool CreateMesh(AActor* Owner);

	/** Shows the decal when active (and on the ground for a snapped ring), and turns the tick on only while snapping. */
	void RefreshState();

	/** Snapped ring: places the decal on the ground under the owner. Returns false when no ground was found. */
	bool UpdateSnap();
};
