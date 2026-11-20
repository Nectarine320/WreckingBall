#include "Match/WB_GoalkeeperZoneComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Match/WB_TeamGoalComponent.h"

TArray<TWeakObjectPtr<UWB_GoalkeeperZoneComponent>> UWB_GoalkeeperZoneComponent::Registry;

UWB_GoalkeeperZoneComponent::UWB_GoalkeeperZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;
	bHiddenInGame = true;
	InitBoxExtent(FVector(300.f, 500.f, 150.f));
}

void UWB_GoalkeeperZoneComponent::OnRegister()
{
	Super::OnRegister();
	Registry.AddUnique(this);
}

void UWB_GoalkeeperZoneComponent::OnUnregister()
{
	Registry.RemoveAll([this](const TWeakObjectPtr<UWB_GoalkeeperZoneComponent>& Zone) { return !Zone.IsValid() || Zone.Get() == this; });
	Super::OnUnregister();
}

int32 UWB_GoalkeeperZoneComponent::GetDefendingTeamIndex() const
{
	const AActor* Owner = GetOwner();
	const UWB_TeamGoalComponent* Goal = Owner ? Owner->FindComponentByClass<UWB_TeamGoalComponent>() : nullptr;
	return Goal ? Goal->DefendingTeamIndex : -1;
}

bool UWB_GoalkeeperZoneComponent::IsLocationInside(FVector Location, float Margin) const
{
	const FVector Local = GetComponentTransform().InverseTransformPositionNoScale(Location);
	const FVector Extent = GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X + Margin && FMath::Abs(Local.Y) <= Extent.Y + Margin;
}

float UWB_GoalkeeperZoneComponent::GetExitDistance(FVector Origin, FVector Direction) const
{
	const FVector Extent = GetScaledBoxExtent();
	const FVector LocalOrigin = GetComponentTransform().InverseTransformPositionNoScale(Origin);
	FVector LocalDirection = GetComponentQuat().UnrotateVector(Direction);
	LocalDirection.Z = 0.f;
	if (!LocalDirection.Normalize() || FMath::Abs(LocalOrigin.X) > Extent.X || FMath::Abs(LocalOrigin.Y) > Extent.Y)
	{
		return 0.f;
	}
	// Ray from inside the box: the nearest of the X and Y slab exits.
	float Exit = TNumericLimits<float>::Max();
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float D = LocalDirection[Axis];
		if (FMath::Abs(D) > KINDA_SMALL_NUMBER)
		{
			Exit = FMath::Min(Exit, (FMath::Sign(D) * Extent[Axis] - LocalOrigin[Axis]) / D);
		}
	}
	return Exit == TNumericLimits<float>::Max() ? 0.f : FMath::Max(0.f, Exit);
}

UWB_GoalkeeperZoneComponent* UWB_GoalkeeperZoneComponent::FindZoneForTeam(const UObject* WorldContextObject, int32 TeamIndex)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || TeamIndex < 0)
	{
		return nullptr;
	}
	for (const TWeakObjectPtr<UWB_GoalkeeperZoneComponent>& Zone : Registry)
	{
		if (Zone.IsValid() && Zone->GetWorld() == World && Zone->GetDefendingTeamIndex() == TeamIndex)
		{
			return Zone.Get();
		}
	}
	return nullptr;
}
