#include "Impact/Destructible/WB_DestructibleLibrary.h"

#include "GameFramework/Actor.h"
#include "Impact/Destructible/WB_DestructibleDependent.h"

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
