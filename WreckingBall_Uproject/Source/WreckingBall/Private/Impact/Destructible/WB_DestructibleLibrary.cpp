#include "Impact/Destructible/WB_DestructibleLibrary.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Impact/Budget/WB_PhysicsBudgetManager_Base.h"
#include "Impact/Destructible/WB_DestructibleDependent.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"

bool UWB_DestructibleLibrary::CollapseGeometryCollection(UGeometryCollectionComponent* Collection, FVector ImpulseOrigin, float ImpulseRadius, float ImpulseStrength, float DebrisLifetime)
{
	if (!IsValid(Collection))
	{
		return false;
	}
	UWorld* World = Collection->GetWorld();
	if (!World)
	{
		return false;
	}

	Collection->SetSimulatePhysics(true);
	Collection->CrumbleActiveClusters();
	Collection->AddRadialImpulse(ImpulseOrigin, ImpulseRadius, ImpulseStrength, ERadialImpulseFalloff::RIF_Constant, true);

	for (TActorIterator<AWB_PhysicsBudgetManager_Base> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->RegisterGeometryCollection(Collection);
			return true;
		}
	}

	// No budget manager in the level: hide the debris after its lifetime.
	if (DebrisLifetime > 0.f)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Collection, [Collection]()
		{
			Collection->SetVisibility(false);
			Collection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Collection->SetSimulatePhysics(false);
		}), DebrisLifetime, false);
	}
	return false;
}

int32 UWB_DestructibleLibrary::GetSideFromNormal(const AActor* Wall, FVector Normal, float MinSideDot)
{
	if (!IsValid(Wall))
	{
		return 0;
	}
	const float SideDot = FVector::DotProduct(Normal.GetSafeNormal(), Wall->GetActorRightVector());
	if (SideDot > MinSideDot)
	{
		return 1;
	}
	if (SideDot < -MinSideDot)
	{
		return -1;
	}
	return 0;
}

TEnumAsByte<EPhysicalSurface> UWB_DestructibleLibrary::GetWallSideSurface(const AActor* Wall, FVector Normal, const UPhysicalMaterial* SideA, const UPhysicalMaterial* SideB, TEnumAsByte<EPhysicalSurface> Fallback)
{
	const int32 Side = GetSideFromNormal(Wall, Normal, 0.5f);
	const UPhysicalMaterial* SideMaterial = Side > 0 ? SideA : (Side < 0 ? SideB : nullptr);
	return SideMaterial ? SideMaterial->SurfaceType : Fallback;
}

FVector UWB_DestructibleLibrary::OrientNormalTowardSource(FVector Normal, FVector ImpactPoint, const AActor* Source)
{
	if (!IsValid(Source))
	{
		return Normal;
	}
	const FVector ToSource = Source->GetActorLocation() - ImpactPoint;
	return FVector::DotProduct(Normal, ToSource) < 0.f ? -Normal : Normal;
}

void UWB_DestructibleLibrary::SyncMaterialOverrides(UMeshComponent* Mesh, TArray<UMaterialInterface*>& Overrides)
{
	if (!IsValid(Mesh))
	{
		return;
	}
	const int32 SlotCount = Mesh->GetNumMaterials();
	Overrides.SetNum(SlotCount);
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		if (!Overrides[Slot])
		{
			// Show the material in use (asset or template) instead of an empty entry.
			Overrides[Slot] = Mesh->GetMaterial(Slot);
		}
		else if (Mesh->GetMaterial(Slot) != Overrides[Slot])
		{
			Mesh->SetMaterial(Slot, Overrides[Slot]);
		}
	}
}

void UWB_DestructibleLibrary::ApplyMaterialOverrides(UMeshComponent* Mesh, const TArray<UMaterialInterface*>& Materials)
{
	if (!IsValid(Mesh))
	{
		return;
	}
	const int32 Count = FMath::Min(Materials.Num(), Mesh->GetNumMaterials());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Materials[Index])
		{
			Mesh->SetMaterial(Index, Materials[Index]);
		}
	}
}

namespace WBDependents
{
	/** The objects to notify on one attached actor: the actor itself and its components, when they implement WB_DestructibleDependent. */
	static void GatherDependentObjects(AActor* Actor, TArray<UObject*>& OutObjects)
	{
		if (!IsValid(Actor))
		{
			return;
		}
		if (Actor->Implements<UWB_DestructibleDependent>())
		{
			OutObjects.Add(Actor);
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (IsValid(Component) && Component->Implements<UWB_DestructibleDependent>())
			{
				OutObjects.Add(Component);
			}
		}
	}

	/** Every dependent object of every actor attached to Parent. Copied first: a dependent may detach itself while it reacts. */
	static void GatherAll(AActor* Parent, TArray<UObject*>& OutObjects)
	{
		if (!IsValid(Parent))
		{
			return;
		}
		TArray<AActor*> Attached;
		Parent->GetAttachedActors(Attached, true, false);
		for (AActor* Actor : Attached)
		{
			GatherDependentObjects(Actor, OutObjects);
		}
	}
}

void UWB_DestructibleLibrary::GetDependents(AActor* Parent, TArray<AActor*>& OutDependents)
{
	OutDependents.Reset();
	if (!IsValid(Parent))
	{
		return;
	}

	TArray<AActor*> Attached;
	Parent->GetAttachedActors(Attached, true, false);
	for (AActor* Actor : Attached)
	{
		TArray<UObject*> Objects;
		WBDependents::GatherDependentObjects(Actor, Objects);
		if (!Objects.IsEmpty())
		{
			OutDependents.Add(Actor);
		}
	}
}

int32 UWB_DestructibleLibrary::NotifyDependentsDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage)
{
	TArray<UObject*> Objects;
	WBDependents::GatherAll(Parent, Objects);
	for (UObject* Object : Objects)
	{
		if (IsValid(Object))
		{
			IWB_DestructibleDependent::Execute_OnParentDamaged(Object, Parent, ImpactLocation, ImpactNormal, Damage);
		}
	}
	return Objects.Num();
}

int32 UWB_DestructibleLibrary::NotifyDependentsDestroyed(AActor* Parent)
{
	TArray<UObject*> Objects;
	WBDependents::GatherAll(Parent, Objects);
	for (UObject* Object : Objects)
	{
		if (IsValid(Object))
		{
			IWB_DestructibleDependent::Execute_OnParentDestroyed(Object, Parent);
		}
	}
	return Objects.Num();
}
