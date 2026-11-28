#include "Match/WB_TeamGoalComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

TArray<TWeakObjectPtr<UWB_TeamGoalComponent>> UWB_TeamGoalComponent::Registry;

UWB_TeamGoalComponent::UWB_TeamGoalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_TeamGoalComponent::OnRegister()
{
	Super::OnRegister();
	Registry.AddUnique(this);
}

void UWB_TeamGoalComponent::OnUnregister()
{
	Registry.RemoveAll([this](const TWeakObjectPtr<UWB_TeamGoalComponent>& Goal) { return !Goal.IsValid() || Goal.Get() == this; });
	Super::OnUnregister();
}

TArray<UWB_TeamGoalComponent*> UWB_TeamGoalComponent::GetAllGoals(const UObject* WorldContextObject)
{
	TArray<UWB_TeamGoalComponent*> Goals;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return Goals;
	}
	for (const TWeakObjectPtr<UWB_TeamGoalComponent>& Goal : Registry)
	{
		if (Goal.IsValid() && Goal->GetWorld() == World)
		{
			Goals.Add(Goal.Get());
		}
	}
	return Goals;
}

FVector UWB_TeamGoalComponent::GetFieldDirection() const
{
	FVector Sum = FVector::ZeroVector;
	for (const UWB_TeamGoalComponent* Other : GetAllGoals(this))
	{
		if (Other != this)
		{
			Sum += (Other->GetComponentLocation() - GetComponentLocation()).GetSafeNormal2D();
		}
	}
	if (Sum.IsNearlyZero())
	{
		Sum = GetForwardVector();
	}
	return Sum.GetSafeNormal2D();
}

UWB_TeamGoalComponent* UWB_TeamGoalComponent::FindGoalDefendedBy(const UObject* WorldContextObject, int32 TeamIndex)
{
	if (TeamIndex < 0)
	{
		return nullptr;
	}
	for (UWB_TeamGoalComponent* Goal : GetAllGoals(WorldContextObject))
	{
		if (Goal->DefendingTeamIndex == TeamIndex)
		{
			return Goal;
		}
	}
	return nullptr;
}

UWB_TeamGoalComponent* UWB_TeamGoalComponent::FindGoalToAttack(const UObject* WorldContextObject, int32 AttackingTeamIndex, FVector FromLocation)
{
	UWB_TeamGoalComponent* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (UWB_TeamGoalComponent* Goal : GetAllGoals(WorldContextObject))
	{
		if (AttackingTeamIndex >= 0 && Goal->DefendingTeamIndex == AttackingTeamIndex)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Goal->GetComponentLocation(), FromLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Goal;
		}
	}
	return Best;
}

FVector UWB_TeamGoalComponent::GetMouthAxis() const
{
	return FVector::CrossProduct(FVector::UpVector, GetFieldDirection()).GetSafeNormal();
}
