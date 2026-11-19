#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "WB_GroundRingProfile.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Settings of a ground ring decal (UI 3D devis, lot A): DA_GroundRing_Player under each player, DA_GroundRing_Ball under the active ball.
 * Read by UWB_GroundRingComponent. The look itself (thickness, shadow, kick gauge colors) is in the decal material instance.
 */
UCLASS(BlueprintType)
class WRECKINGBALL_API UWB_GroundRingProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Decal material (Deferred Decal domain, M_GroundRing parent). Required when bUseMesh is off: empty, no ring is created and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	/** Radius of the ring on the ground, in cm (half the width of the decal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing", meta = (ClampMin = "1", Units = "Centimeters"))
	float Radius = 60.f;

	/** Half height of the projection box, in cm: the ring is drawn on surfaces up to this distance above or below its center (steps, rugs, low furniture). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing", meta = (ClampMin = "1", Units = "Centimeters"))
	float ProjectionHalfHeight = 50.f;

	/** Draw order against the other decals: higher draws on top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing")
	int32 SortOrder = 10;

	/** On: the ring shows at BeginPlay (players). Off: hidden until SetRingActive(true) (active ball marker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing")
	bool bStartActive = true;

	// ---- Mesh mode ----

	/**
	 * Off (default): the ring is a decal projected on the ground (wraps on steps and rugs, but TSR ghosts it when it moves: a decal writes no motion vectors).
	 * On: the ring is a flat mesh (RingMesh) with a translucent material that outputs velocity and uses Responsive AA: no ghosting, but it stays flat
	 * (it does not wrap on a step edge). Uses MeshMaterial instead of DecalMaterial, and ignores ProjectionHalfHeight and SortOrder.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Mesh")
	bool bUseMesh = false;

	/** Flat mesh facing up, centered on its pivot (/Engine/BasicShapes/Plane). Scaled so its width matches 2 x Radius. Required in mesh mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Mesh", meta = (EditCondition = "bUseMesh"))
	TObjectPtr<UStaticMesh> RingMesh = nullptr;

	/** Surface material, translucent and unlit, with Output Velocity and Responsive AA (M_GroundRing_Mesh parent): MI_GroundRing_Player, MI_GroundRing_Ball. Required in mesh mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Mesh", meta = (EditCondition = "bUseMesh"))
	TObjectPtr<UMaterialInterface> MeshMaterial = nullptr;

	/** Height of the mesh above the ground, in cm: keeps it from flickering through the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Mesh", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bUseMesh"))
	float MeshHeightOffset = 1.f;

	/** Draw order against the other translucent objects in mesh mode: higher draws on top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Mesh", meta = (EditCondition = "bUseMesh"))
	int32 MeshSortPriority = 10;

	// ---- Snap to ground ----

	/**
	 * Off (default): the decal stays at the component (under the feet of a player).
	 * On: every frame while active, a line trace goes down from the owner and the decal is placed on the ground hit, at the owner's X and Y,
	 * so the ring stays on the ground when the owner flies (ball). Hidden while no ground is found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap")
	bool bSnapToGround = false;

	/** Length of the downward trace, in cm. Ground farther than this below the owner: the ring hides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (ClampMin = "1", Units = "Centimeters", EditCondition = "bSnapToGround"))
	float TraceLength = 3000.f;

	/** Object types the trace can land on. WorldStatic only (default) ignores the ball, the players, the debris and the moving physics objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (EditCondition = "bSnapToGround"))
	TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjectTypes;

	/** On: the shadow of the ring (ShadowStrength) fades as the owner rises above the ground; the outline stays. Off (default): constant look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (EditCondition = "bSnapToGround"))
	bool bFadeShadowWithHeight = false;

	/** Height above the ground, in cm, where the shadow starts to fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bSnapToGround && bFadeShadowWithHeight"))
	float FadeStartHeight = 100.f;

	/** Height above the ground, in cm, where the shadow reaches MinShadowFactor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (ClampMin = "0", Units = "Centimeters", EditCondition = "bSnapToGround && bFadeShadowWithHeight"))
	float FadeEndHeight = 800.f;

	/** Shadow strength left at FadeEndHeight and above, as a fraction of the material's ShadowStrength (0 = no shadow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroundRing|Snap", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bSnapToGround && bFadeShadowWithHeight"))
	float MinShadowFactor = 0.3f;

	UWB_GroundRingProfile();
};
