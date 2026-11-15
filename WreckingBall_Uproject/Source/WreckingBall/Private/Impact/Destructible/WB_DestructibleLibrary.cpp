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
		if (IsValid(Actor) && Actor->Implements<UWB_DestructibleDependent>())
		{
			OutDependents.Add(Actor);
		}
	}
}

int32 UWB_DestructibleLibrary::NotifyDependentsDamaged(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage)
{
	// Copy first: a dependent may detach itself while it reacts.
	TArray<AActor*> Dependents;
	GetDependents(Parent, Dependents);
	for (AActor* Dependent : Dependents)
	{
		if (IsValid(Dependent))
		{
			IWB_DestructibleDependent::Execute_OnParentDamaged(Dependent, Parent, ImpactLocation, ImpactNormal, Damage);
		}
	}
	return Dependents.Num();
}

int32 UWB_DestructibleLibrary::NotifyDependentsDestroyed(AActor* Parent)
{
	TArray<AActor*> Dependents;
	GetDependents(Parent, Dependents);
	for (AActor* Dependent : Dependents)
	{
		if (IsValid(Dependent))
		{
			IWB_DestructibleDependent::Execute_OnParentDestroyed(Dependent, Parent);
		}
	}
	return Dependents.Num();
}
