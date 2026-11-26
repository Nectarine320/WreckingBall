#include "Feedback/XRay/WB_XRayOutlineComponent.h"

#include "Feedback/XRay/WB_XRayOutlineProfile.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/ShapeComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBXRayOutline, Log, All);

namespace
{
	/** Technical cap: invisible blockers skipped on one trace before giving up (counted as not blocked). */
	constexpr int32 MaxInvisibleSkips = 8;

	/** True when the component is drawn on screen, so it can really hide what is behind it. Collision shapes and hidden components or actors cannot. */
	bool IsRenderedInGame(const UPrimitiveComponent* Component)
	{
		if (Component->IsA<UShapeComponent>() || !Component->IsVisible() || Component->bHiddenInGame)
		{
			return false;
		}
		const AActor* Owner = Component->GetOwner();
		return !(Owner && Owner->IsHidden());
	}
}

UWB_XRayOutlineComponent::UWB_XRayOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_XRayOutlineComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Targets.Num() == 0)
	{
		CollectDefaultTargets();
	}

	if (bEnabledOnBeginPlay)
	{
		SetXRayEnabled(true);
	}
}

void UWB_XRayOutlineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetXRayEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UWB_XRayOutlineComponent::CollectDefaultTargets()
{
	Targets.Reset();
	if (const AActor* Owner = GetOwner())
	{
		TArray<UMeshComponent*> Meshes;
		Owner->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			Targets.Add(Mesh);
		}
	}
}

void UWB_XRayOutlineComponent::SetTargetComponents(const TArray<UPrimitiveComponent*>& NewTargets)
{
	const bool bWasOccluded = bOccluded;
	ApplyOccluded(false);

	Targets.Reset();
	for (UPrimitiveComponent* Target : NewTargets)
	{
		if (Target)
		{
			Targets.Add(Target);
		}
	}
	if (Targets.Num() == 0)
	{
		CollectDefaultTargets();
	}

	ApplyOccluded(bWasOccluded);
}

void UWB_XRayOutlineComponent::SetXRayEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bEnabled)
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
		bXRayEnabled = false;
		ApplyOccluded(false);
		return;
	}

	if (!Profile)
	{
		UE_LOG(LogWBXRayOutline, Warning, TEXT("%s: no XRay Outline Profile set, the silhouette stays off."), *GetNameSafe(GetOwner()));
		return;
	}

	bXRayEnabled = true;
	World->GetTimerManager().SetTimer(CheckTimer, this, &UWB_XRayOutlineComponent::CheckOcclusion, FMath::Max(Profile->CheckInterval, 0.01f), true);
	CheckOcclusion();
}

void UWB_XRayOutlineComponent::CheckOcclusion()
{
	if (!Profile)
	{
		return;
	}

	bool bValid = false;
	const bool bNewOccluded = ComputeOccluded(bValid);
	if (!bValid || bNewOccluded == bOccluded)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastStateChangeTime < Profile->MinStateDuration)
	{
		return;
	}

	ApplyOccluded(bNewOccluded);
}

bool UWB_XRayOutlineComponent::ComputeOccluded(bool& bOutValid) const
{
	bOutValid = false;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0); // The shared camera.
	if (!World || !Owner || !Camera)
	{
		return false;
	}

	// Center and radius from the target meshes.
	FBoxSphereBounds Bounds(ForceInit);
	bool bHasBounds = false;
	for (const UPrimitiveComponent* Target : Targets)
	{
		if (Target && Target->IsVisible())
		{
			Bounds = bHasBounds ? Bounds + Target->Bounds : Target->Bounds;
			bHasBounds = true;
		}
	}
	if (!bHasBounds)
	{
		return false;
	}

	const FVector CameraLocation = Camera->GetCameraLocation();
	const FVector Center = Bounds.Origin;
	const FVector ViewDir = (Center - CameraLocation).GetSafeNormal();
	if (ViewDir.IsNearlyZero())
	{
		return false;
	}

	// Ring of edge points, facing the camera.
	FVector Right = FVector::CrossProduct(ViewDir, FVector::UpVector).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}
	const FVector Up = FVector::CrossProduct(Right, ViewDir).GetSafeNormal();
	const float RingRadius = Bounds.SphereRadius * Profile->EdgeSampleRadiusScale;

	TArray<FVector, TInlineAllocator<17>> Points;
	Points.Add(Center);
	const int32 EdgeCount = FMath::Clamp(Profile->EdgeSampleCount, 0, 16);
	for (int32 Index = 0; Index < EdgeCount; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / EdgeCount;
		Points.Add(Center + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * RingRadius);
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBXRayOutline), false, Owner);
	if (Profile->bIgnorePawns)
	{
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
	}

	int32 BlockedCount = 0;
	for (const FVector& Point : Points)
	{
		FHitResult Hit;
		bool bBlocked = false;
		FCollisionQueryParams PointParams = Params;
		for (int32 Pass = 0; Pass < MaxInvisibleSkips; ++Pass)
		{
			bBlocked = World->LineTraceSingleByChannel(Hit, CameraLocation, Point, Profile->TraceChannel, PointParams);
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!bBlocked || !HitComponent || IsRenderedInGame(HitComponent))
			{
				break;
			}
			// Invisible blocker (play area box, hidden wall...): it cannot hide the actor, trace through it.
			PointParams.AddIgnoredComponent(HitComponent);
			bBlocked = false;
		}
		BlockedCount += bBlocked ? 1 : 0;

#if ENABLE_DRAW_DEBUG
		if (bDebugDrawTraces)
		{
			DrawDebugLine(World, CameraLocation, bBlocked ? Hit.ImpactPoint : Point, bBlocked ? FColor::Red : FColor::Green, false, Profile->CheckInterval, 0, 1.f);
		}
#endif
	}

	bOutValid = true;
	const int32 Needed = FMath::Max(1, FMath::CeilToInt(Points.Num() * FMath::Clamp(Profile->OccludedRatio, 0.01f, 1.f)));
	return BlockedCount >= Needed;
}

void UWB_XRayOutlineComponent::ApplyOccluded(bool bNewOccluded)
{
	const bool bChanged = bNewOccluded != bOccluded;
	bOccluded = bNewOccluded;

	for (UPrimitiveComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		if (bNewOccluded && Profile)
		{
			Target->SetCustomDepthStencilValue(FMath::Clamp(Profile->StencilValue, 0, 255));
		}
		if (Target->bRenderCustomDepth != bNewOccluded)
		{
			Target->SetRenderCustomDepth(bNewOccluded);
		}
	}

	if (bChanged)
	{
		if (const UWorld* World = GetWorld())
		{
			LastStateChangeTime = World->GetTimeSeconds();
		}
		OnOcclusionChanged.Broadcast(bNewOccluded);
	}
}
