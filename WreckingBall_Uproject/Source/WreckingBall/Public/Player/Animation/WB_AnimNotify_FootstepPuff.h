#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WB_AnimNotify_FootstepPuff.generated.h"

class UNiagaraSystem;

/**
 * Footstep dust puff: place it on each foot contact of a walk / run animation (one notify per foot).
 * Spawns PuffSystem at FootBone, turned toward the owner's movement, so NS_FootstepPuff (emitting along its local -X) puffs behind the player.
 * Nothing spawns while the owner moves slower than MinSpeed (blended idle frames, standing still).
 */
UCLASS(meta = (DisplayName = "Footstep Puff"))
class WRECKINGBALL_API UWB_AnimNotify_FootstepPuff : public UAnimNotify
{
	GENERATED_BODY()

public:
	UWB_AnimNotify_FootstepPuff();

	/** Niagara system spawned at the foot (NS_FootstepPuff). It should emit backward along its local -X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	TSoftObjectPtr<UNiagaraSystem> PuffSystem;

	/** Bone (or socket) where the puff appears: Foot_l or Foot_r on the prototype skeleton. None: the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	FName FootBone = TEXT("Foot_l");

	/** Minimum horizontal speed in cm/s of the owner for a puff. Below it (standing, idle blend), nothing spawns. Ignored in the animation editor preview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MinSpeed = 50.f;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
