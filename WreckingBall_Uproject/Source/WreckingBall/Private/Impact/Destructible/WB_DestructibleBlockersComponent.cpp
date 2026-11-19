#include "Impact/Destructible/WB_DestructibleBlockersComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBBlockers, Log, All);

UWB_DestructibleBlockersComponent::UWB_DestructibleBlockersComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_DestructibleBlockersComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshBlockers();
}

int32 UWB_DestructibleBlockersComponent::RefreshBlockers()
{
	for (UPrimitiveComponent* Blocker : Blockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->OnComponentHit.RemoveDynamic(this, &UWB_DestructibleBlockersComponent::HandleBlockerHit);
		}
	}
	Blockers.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}

	TArray<UPrimitiveComponent*> Found;
	Owner->GetComponents<UPrimitiveComponent>(Found);
	for (UPrimitiveComponent* Component : Found)
	{
		if (IsValid(Component) && Component->ComponentHasTag(BlockerTag))
		{
			Component->SetNotifyRigidBodyCollision(true);
			Component->OnComponentHit.AddUniqueDynamic(this, &UWB_DestructibleBlockersComponent::HandleBlockerHit);
			Blockers.Add(Component);
		}
	}

	if (Blockers.IsEmpty())
	{
		UE_LOG(LogWBBlockers, Warning, TEXT("%s: no component with the tag %s. The actor receives no hit; add a blocker box with this tag."),
			*Owner->GetName(), *BlockerTag.ToString());
	}
	return Blockers.Num();
}

void UWB_DestructibleBlockersComponent::SetBlockersCollisionEnabled(ECollisionEnabled::Type NewType)
{
	for (UPrimitiveComponent* Blocker : Blockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->SetCollisionEnabled(NewType);
		}
	}
}

TArray<UPrimitiveComponent*> UWB_DestructibleBlockersComponent::GetBlockers() const
{
	TArray<UPrimitiveComponent*> Result;
	Result.Reserve(Blockers.Num());
	for (UPrimitiveComponent* Blocker : Blockers)
	{
		Result.Add(Blocker);
	}
	return Result;
}

void UWB_DestructibleBlockersComponent::HandleBlockerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	if (OtherActor && Owner && (OtherActor == Owner || OtherActor->IsAttachedTo(Owner)))
	{
		return;
	}
	OnBlockerHit.Broadcast(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
}
