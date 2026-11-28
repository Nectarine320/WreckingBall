#include "Match/WB_TeamZoneComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

TArray<TWeakObjectPtr<UWB_TeamZoneComponent>> UWB_TeamZoneComponent::Registry;

UWB_TeamZoneComponent::UWB_TeamZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;
	bHiddenInGame = true;
	InitBoxExtent(FVector(1000.f, 1000.f, 200.f));
}

void UWB_TeamZoneComponent::OnRegister()
{
	Super::OnRegister();
	Registry.AddUnique(this);
}

void UWB_TeamZoneComponent::OnUnregister()
{
	Registry.RemoveAll([this](const TWeakObjectPtr<UWB_TeamZoneComponent>& Zone) { return !Zone.IsValid() || Zone.Get() == this; });
	Super::OnUnregister();
}

bool UWB_TeamZoneComponent::IsLocationInside(FVector Location, float Margin) const
{
	const FVector Local = GetComponentTransform().InverseTransformPositionNoScale(Location);
	const FVector Extent = GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X + Margin && FMath::Abs(Local.Y) <= Extent.Y + Margin;
}

UWB_TeamZoneComponent* UWB_TeamZoneComponent::FindZoneForTeam(const UObject* WorldContextObject, int32 InTeamIndex)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || InTeamIndex < 0)
	{
		return nullptr;
	}
	for (const TWeakObjectPtr<UWB_TeamZoneComponent>& Zone : Registry)
	{
		if (Zone.IsValid() && Zone->GetWorld() == World && Zone->TeamIndex == InTeamIndex)
		{
			return Zone.Get();
		}
	}
	return nullptr;
}
