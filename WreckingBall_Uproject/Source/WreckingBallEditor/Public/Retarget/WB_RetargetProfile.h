// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WB_RetargetProfile.generated.h"

class USkeletalMesh;

/** A bone name to replace in the anim notifies of the retargeted animations (the source skeleton names its bones differently). */
USTRUCT(BlueprintType)
struct FWB_NotifyBoneRename
{
	GENERATED_BODY()

	/** Bone name of the source skeleton, as written in the notifies (case ignored), e.g. Foot_l. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	FName From;

	/** Bone name of the target skeleton, e.g. LeftFoot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	FName To;
};

/** A bone rotation added to the retargeted animations whose name matches, to fix a pose the retarget cannot (arms of a big-headed character...). */
USTRUCT(BlueprintType)
struct FWB_BoneRotationFix
{
	GENERATED_BODY()

	/** The fix applies to every retargeted Anim Sequence whose name contains this text, e.g. AS_Dive_Air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	FString AnimNameContains;

	/** Bone of the target skeleton, e.g. LeftArm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	FName Bone;

	/** Rotation added in the bone's local space, in degrees, at full weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	FRotator Offset = FRotator::ZeroRotator;

	/** First key affected, from 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (ClampMin = "0"))
	int32 StartFrame = 0;

	/** Last key affected. -1 = the last key of the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (ClampMin = "-1"))
	int32 EndFrame = -1;

	/** Keys over which the offset grows from 0 to full, from StartFrame. 0 = full at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (ClampMin = "0"))
	int32 BlendInFrames = 0;

	/** Keys over which the offset fades back to 0, ending at EndFrame. 0 = full until the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (ClampMin = "0"))
	int32 BlendOutFrames = 0;

	/** Why this fix exists, for the team. Not used by the tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (MultiLine = true))
	FText Note;
};

/**
 * Touch-ups of one target character, applied after every retarget onto it (WB Tools > Animation > Retarget): a retarget that overwrites
 * the animations erases manual fixes, the profile redoes them. One profile per target character (DA_RetargetProfile_<Character>).
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Tools"))
class WRECKINGBALLEDITOR_API UWB_RetargetProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Target character of this profile: the Retarget panel picks this profile when this mesh is the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget")
	TSoftObjectPtr<USkeletalMesh> TargetMesh;

	/** Bone names replaced in the anim notifies (footstep bones...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (TitleProperty = "{From} -> {To}"))
	TArray<FWB_NotifyBoneRename> NotifyBoneRenames;

	/** Bone rotations added to the matching animations. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retarget", meta = (TitleProperty = "{AnimNameContains} - {Bone}"))
	TArray<FWB_BoneRotationFix> RotationFixes;

	/**
	 * Applies the touch-ups to retargeted assets (Anim Sequences, Montages; other assets are skipped). The assets are not saved.
	 * @param Assets Assets created by the retarget.
	 * @param OutLog One line per change, and the problems.
	 * @return Number of changes.
	 */
	int32 ApplyFixes(const TArray<UObject*>& Assets, TArray<FString>& OutLog) const;

	/** Profile whose TargetMesh is this mesh, null when none. */
	static UWB_RetargetProfile* FindForMesh(const USkeletalMesh* Mesh);
};
