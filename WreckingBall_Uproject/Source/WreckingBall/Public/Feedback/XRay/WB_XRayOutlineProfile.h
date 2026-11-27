#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "WB_XRayOutlineProfile.generated.h"

/**
 * X-ray outline settings (2026-11-26): when an actor is hidden from the shared camera by a wall or furniture, its meshes are drawn
 * in Custom Depth with a stencil value, and the map's Post Process Volume (M_Outliner_Balls_Master_Inst) paints the silhouette.
 * Read by UWB_XRayOutlineComponent. One profile per kind of actor (DA_XRayOutline_Ball, later players...).
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Feedback"))
class WRECKINGBALL_API UWB_XRayOutlineProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Detection ----

	/** Time in seconds between two occlusion checks. Lower reacts faster, higher costs less. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection", meta = (ClampMin = "0.01", Units = "Seconds"))
	float CheckInterval = 0.05f;

	/** Trace channel used from the camera to the actor. XRayOcclusion: blocks by default, ignored by players, invisible walls, triggers and destructible blocker boxes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel2;

	/** Ignores every pawn (players and bots) in the trace, even one with a custom collision that blocks XRayOcclusion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection")
	bool bIgnorePawns = true;

	/** Extra points traced on a ring around the center, facing the camera. 0: center only. More points: a half-hidden actor is judged better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection", meta = (ClampMin = "0", ClampMax = "16"))
	int32 EdgeSampleCount = 4;

	/** Radius of the ring of edge points, as a fraction of the actor's bounds radius. 1: on the silhouette edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection", meta = (ClampMin = "0", ClampMax = "1.5"))
	float EdgeSampleRadiusScale = 0.7f;

	/** Fraction of traced points (center + edges) that must be blocked to show the silhouette. 1: fully hidden only. 0.5: half hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection", meta = (ClampMin = "0.01", ClampMax = "1"))
	float OccludedRatio = 0.6f;

	/** Minimum time in seconds the silhouette stays on or off before it can change again. Prevents flicker behind thin furniture legs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Detection", meta = (ClampMin = "0", Units = "Seconds"))
	float MinStateDuration = 0.1f;

	// ---- Render ----

	/** Custom Stencil value written by the meshes while hidden. Must match the value read by the Post Process material (1 for M_Outliner_Balls_Master). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XRayOutline|Render", meta = (ClampMin = "0", ClampMax = "255"))
	int32 StencilValue = 1;
};
