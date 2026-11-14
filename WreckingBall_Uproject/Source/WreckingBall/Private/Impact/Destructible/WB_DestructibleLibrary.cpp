#include "Impact/Destructible/WB_DestructibleLibrary.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Impact/Destructible/WB_DestructibleDependent.h"
#include "Materials/MaterialInterface.h"

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
