#include "Impact/Destructible/WB_DestructibleDependentComponent.h"

UWB_DestructibleDependentComponent::UWB_DestructibleDependentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_DestructibleDependentComponent::OnParentDamaged_Implementation(AActor* Parent, FVector ImpactLocation, FVector ImpactNormal, float Damage)
{
	ParentDamaged.Broadcast(Parent, ImpactLocation, ImpactNormal, Damage);
}

void UWB_DestructibleDependentComponent::OnParentDestroyed_Implementation(AActor* Parent)
{
	ParentDestroyed.Broadcast(Parent);
}
