#include "Player/Animation/WB_AnimNotify_FootstepPuff.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UWB_AnimNotify_FootstepPuff::UWB_AnimNotify_FootstepPuff()
{
	PuffSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Art/VFX/Smoke_01/NS_FootstepPuff.NS_FootstepPuff")));
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(200, 180, 140);
#endif
}

FString UWB_AnimNotify_FootstepPuff::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Footstep Puff (%s)"), *FootBone.ToString());
}

void UWB_AnimNotify_FootstepPuff::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}
	UNiagaraSystem* System = PuffSystem.LoadSynchronous();
	if (!System)
	{
		return;
	}

	// Face the movement so the puff (emitted along the system's local -X) goes behind the player.
	// In the animation editor preview the owner does not move: use its forward and always spawn.
	const AActor* Owner = MeshComp->GetOwner();
	const UWorld* World = MeshComp->GetWorld();
	const bool bPreview = !World || !World->IsGameWorld();
	FVector Direction = Owner ? Owner->GetVelocity().GetSafeNormal2D() : FVector::ZeroVector;
	if (!bPreview && (!Owner || Owner->GetVelocity().Size2D() < MinSpeed))
	{
		return;
	}
	if (Direction.IsNearlyZero())
	{
		Direction = (Owner ? Owner->GetActorForwardVector() : MeshComp->GetForwardVector()).GetSafeNormal2D();
	}

	const FVector Location = FootBone.IsNone() ? MeshComp->GetComponentLocation() : MeshComp->GetSocketLocation(FootBone);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp, System, Location, FRotator(0.f, Direction.Rotation().Yaw, 0.f));
}
