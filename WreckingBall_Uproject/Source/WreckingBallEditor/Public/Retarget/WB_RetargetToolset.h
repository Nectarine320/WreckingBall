// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RetargetEditor/IKRetargeterPoseGenerator.h"
#include "Retargeter/IKRetargetSettings.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "WB_RetargetToolset.generated.h"

class UAnimSequence;
class UIKRetargeter;
class UWB_RetargetProfile;
class UIKRigDefinition;
class USkeletalMesh;

/** One retarget chain of an IK Rig: a named run of bones that animation is copied along. */
USTRUCT(BlueprintType)
struct FWB_RetargetChainInfo
{
	GENERATED_BODY()

	/** Name used to pair this chain with a chain of the other skeleton. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName ChainName;

	/** First bone of the chain (closest to the root). */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName StartBone;

	/** Last bone of the chain. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName EndBone;

	/** IK goal attached to the chain. None when the chain is FK only. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName Goal;
};

/** Retarget setup of an IK Rig: its root (pelvis) bone and its chains. */
USTRUCT(BlueprintType)
struct FWB_IKRigRetargetInfo
{
	GENERATED_BODY()

	/** Pelvis bone whose translation drives the whole body. None when unset. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName RetargetRoot;

	/** Retarget chains, in the rig's order. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	TArray<FWB_RetargetChainInfo> Chains;
};

/** Which source chain a target chain copies its animation from. */
USTRUCT(BlueprintType)
struct FWB_RetargetChainPair
{
	GENERATED_BODY()

	/** Chain of the target IK Rig. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName TargetChain;

	/** Chain of the source IK Rig it copies from. None when the target chain is not animated. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	FName SourceChain;
};

/** How much of the source pelvis translation the retargeter copies onto the target pelvis. */
USTRUCT(BlueprintType)
struct FWB_PelvisTranslationScale
{
	GENERATED_BODY()

	/** Scale of the motion on the ground plane (X, Y). 0 keeps the pelvis in place. */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	double Horizontal = 1.0;

	/** Scale of the vertical motion (Z). */
	UPROPERTY(BlueprintReadWrite, Category = "Retarget")
	double Vertical = 1.0;
};

/**
 * Transfers animation between skeletons. Builds IK Rigs (retarget root and chains) and IK Retargeters
 * (chain mapping and retarget pose), then duplicates and retargets animation assets, Blend Spaces and
 * Animation Blueprints onto another skeletal mesh, and touches up bone rotations of the results.
 */
UCLASS(BlueprintType, Hidden)
class UWB_RetargetToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- IK Rig

	/**
	 * Creates an empty IK Rig asset bound to a skeletal mesh.
	 * @param FolderPath Content folder, e.g. "/Game/Art/Character/Retarget".
	 * @param AssetName Name of the new asset. Raises if an asset already has this path.
	 * @param SkeletalMesh Mesh whose skeleton the rig describes.
	 * @return The new IK Rig.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static UIKRigDefinition* CreateIKRig(const FString& FolderPath, const FString& AssetName, USkeletalMesh* SkeletalMesh);

	/**
	 * Replaces the rig's retarget root and chains with the ones Unreal's auto-characterizer generates.
	 * Raises when the skeleton matches no known template (e.g. a custom skeleton); build the chains
	 * with SetRetargetRoot and AddRetargetChain instead.
	 * @param IKRig The rig to set up.
	 * @return The generated retarget setup.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static FWB_IKRigRetargetInfo AutoGenerateRetargetChains(UIKRigDefinition* IKRig);

	/**
	 * Returns the rig's retarget root and chains.
	 * @param IKRig The rig to read.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static FWB_IKRigRetargetInfo GetRetargetDefinition(UIKRigDefinition* IKRig);

	/**
	 * Sets the rig's retarget root, the pelvis bone whose translation drives the whole body.
	 * @param IKRig The rig to edit.
	 * @param BoneName A bone of the rig's skeleton.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static void SetRetargetRoot(UIKRigDefinition* IKRig, FName BoneName);

	/**
	 * Adds an FK retarget chain to the rig.
	 * @param IKRig The rig to edit.
	 * @param ChainName Wanted name. Made unique if another chain already uses it.
	 * @param StartBone First bone of the chain (closest to the root).
	 * @param EndBone Last bone of the chain. Must be StartBone or one of its descendants.
	 * @return The name the chain was given.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static FName AddRetargetChain(UIKRigDefinition* IKRig, FName ChainName, FName StartBone, FName EndBone);

	/**
	 * Changes the bones of an existing retarget chain. Keeps its name, so retargeter mappings stay valid.
	 * @param IKRig The rig to edit.
	 * @param ChainName The chain to change.
	 * @param StartBone New first bone.
	 * @param EndBone New last bone.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static void SetRetargetChainBones(UIKRigDefinition* IKRig, FName ChainName, FName StartBone, FName EndBone);

	/**
	 * Removes a retarget chain from the rig.
	 * @param IKRig The rig to edit.
	 * @param ChainName The chain to remove.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Rig")
	static void RemoveRetargetChain(UIKRigDefinition* IKRig, FName ChainName);

	// ---------------------------------------------------------------- IK Retargeter

	/**
	 * Creates an IK Retargeter between two IK Rigs, set up like the editor's batch retarget window:
	 * default op stack, chains paired by exact name, IK pass off, root motion set from the rigs' roots.
	 * The retarget pose is left untouched; see AutoAlignRetargetPose.
	 * @param FolderPath Content folder, e.g. "/Game/Art/Character/Retarget".
	 * @param AssetName Name of the new asset. Raises if an asset already has this path.
	 * @param SourceIKRig Rig to copy animation from. Must have a preview mesh.
	 * @param TargetIKRig Rig to copy animation to. Must have a preview mesh.
	 * @return The new retargeter.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static UIKRetargeter* CreateRetargeter(const FString& FolderPath, const FString& AssetName, UIKRigDefinition* SourceIKRig, UIKRigDefinition* TargetIKRig);

	/**
	 * Returns, for every chain of the target rig, the source chain it copies from (FK chains op).
	 * @param Retargeter The retargeter to read.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static TArray<FWB_RetargetChainPair> GetChainMapping(UIKRetargeter* Retargeter);

	/**
	 * Sets which source chain a target chain copies from, in every op of the stack.
	 * @param Retargeter The retargeter to edit.
	 * @param TargetChain A chain of the target rig.
	 * @param SourceChain A chain of the source rig, or None to leave the target chain unanimated.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void SetChainMapping(UIKRetargeter* Retargeter, FName TargetChain, FName SourceChain);

	/**
	 * Returns how the FK chains op copies animation onto a target chain.
	 * @param Retargeter The retargeter to read.
	 * @param TargetChain A chain of the target rig.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static FRetargetFKChainSettings GetFKChainSettings(UIKRetargeter* Retargeter, FName TargetChain);

	/**
	 * Sets how the FK chains op copies animation onto a target chain. The chain is the one named in
	 * Settings.TargetChainName. A RotationAlpha below 1 makes a chain follow only part of its source
	 * rotation, e.g. a clavicle mapped to the source arm that rises a little when the arm goes up.
	 * @param Retargeter The retargeter to edit.
	 * @param Settings The new settings of the chain.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void SetFKChainSettings(UIKRetargeter* Retargeter, const FRetargetFKChainSettings& Settings);

	/**
	 * Returns how much of the source pelvis translation the pelvis motion op copies onto the target.
	 * @param Retargeter The retargeter to read.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static FWB_PelvisTranslationScale GetPelvisTranslationScale(UIKRetargeter* Retargeter);

	/**
	 * Sets how much of the source pelvis translation the pelvis motion op copies onto the target.
	 * Horizontal 0 keeps the pelvis at its retarget-pose position on the ground plane (in-place
	 * animations); Vertical 1 keeps hops and crouches.
	 * @param Retargeter The retargeter to edit.
	 * @param Scale The new scales. 1 copies the source motion, 0 removes it.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void SetPelvisTranslationScale(UIKRetargeter* Retargeter, const FWB_PelvisTranslationScale& Scale);

	/**
	 * Rotates bones of the current retarget pose so their direction matches the other skeleton's
	 * retarget pose (e.g. brings a T-pose's arms down to match an A-pose).
	 * @param Retargeter The retargeter to edit.
	 * @param Side Skeleton whose retarget pose is changed.
	 * @param Method How bone directions are measured. ChainToChain suits most bipeds.
	 * @param Bones Bones to align. Empty aligns every mapped bone after resetting the whole pose.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void AutoAlignRetargetPose(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, ERetargetAutoAlignMethod Method, const TArray<FName>& Bones);

	/**
	 * Returns a bone's rotation offset in the current retarget pose, relative to its reference pose.
	 * @param Retargeter The retargeter to read.
	 * @param Side Skeleton the bone belongs to.
	 * @param BoneName The bone to read.
	 * @return Local-space offset in degrees. Zero when the bone is not posed.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static FRotator GetRetargetPoseBoneRotation(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, FName BoneName);

	/**
	 * Sets a bone's rotation offset in the current retarget pose. Replaces any previous offset.
	 * @param Retargeter The retargeter to edit.
	 * @param Side Skeleton the bone belongs to.
	 * @param BoneName The bone to pose.
	 * @param Rotation Local-space offset from the reference pose, in degrees.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void SetRetargetPoseBoneRotation(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, FName BoneName, FRotator Rotation);

	/**
	 * Puts bones of the current retarget pose back to their reference pose.
	 * @param Retargeter The retargeter to edit.
	 * @param Side Skeleton whose pose is reset.
	 * @param Bones Bones to reset. Empty resets every bone and the root offset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|IK Retargeter")
	static void ResetRetargetPose(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, const TArray<FName>& Bones);

	// ---------------------------------------------------------------- Batch

	/**
	 * Duplicates animation assets onto the retargeter's target mesh and retargets them, from the
	 * retargeter's source preview mesh to its target preview mesh. New assets are not saved.
	 * Target bones whose reference pose is scaled (e.g. a Blender root at scale 100) keep that scale.
	 * @param Retargeter The retargeter to use.
	 * @param Assets Anim Sequences, Montages, Blend Spaces or Animation Blueprints of the source skeleton.
	 * @param TargetFolder Content folder that receives the new assets.
	 * @param Search Text replaced by Replace in each new asset's name. Empty to skip.
	 * @param Replace Replacement for Search.
	 * @param Prefix Added before each new asset's name.
	 * @param Suffix Added after each new asset's name.
	 * @param bIncludeReferencedAssets Also retarget the animations a Blend Space or Animation Blueprint uses, and point the copies at them.
	 * @param bOverwriteExisting Overwrite assets that already have the new name, instead of adding a number.
	 * @return Every asset created, referenced assets included.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|Batch")
	static TArray<UObject*> RetargetAnimations(
		UIKRetargeter* Retargeter,
		const TArray<UObject*>& Assets,
		const FString& TargetFolder,
		const FString& Search,
		const FString& Replace,
		const FString& Prefix,
		const FString& Suffix,
		bool bIncludeReferencedAssets,
		bool bOverwriteExisting);

	// ---------------------------------------------------------------- Fix

	/**
	 * Returns a bone's local rotation in an animation at a given frame.
	 * @param Sequence The animation to read.
	 * @param BoneName A bone of the animation's skeleton.
	 * @param Frame Key index, from 0.
	 * @return Rotation relative to the parent bone, in degrees. The reference pose when the bone has no track.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|Fix")
	static FRotator GetBoneLocalRotation(UAnimSequence* Sequence, FName BoneName, int32 Frame);

	/**
	 * Adds a rotation to a bone over a frame range of an animation, to touch up a retargeted pose.
	 * The offset is applied in the bone's own local space, after its animated rotation, and is faded
	 * in and out linearly. The asset is not saved.
	 * @param Sequence The animation to edit.
	 * @param BoneName A bone of the animation's skeleton.
	 * @param Offset Rotation added at full weight, in degrees.
	 * @param StartFrame First key affected, from 0.
	 * @param EndFrame Last key affected. Negative means the last key.
	 * @param BlendInFrames Keys over which the offset grows from 0 to full, starting at StartFrame.
	 * @param BlendOutFrames Keys over which the offset shrinks back to 0, ending at EndFrame.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|Fix")
	static void OffsetBoneRotation(UAnimSequence* Sequence, FName BoneName, FRotator Offset, int32 StartFrame, int32 EndFrame, int32 BlendInFrames, int32 BlendOutFrames);

	/**
	 * Applies the touch-ups of a retarget profile (notify bone renames, bone rotation fixes) to retargeted assets. The assets are not saved.
	 * @param Profile Retarget profile of the target character (DA_RetargetProfile_*).
	 * @param Assets Assets created by RetargetAnimations.
	 * @return One line per change, and the problems.
	 */
	UFUNCTION(meta = (AICallable), Category = "Retarget|Fix")
	static TArray<FString> ApplyRetargetProfile(UWB_RetargetProfile* Profile, const TArray<UObject*>& Assets);

private:
	/** Raises a script error that the toolset registry returns to the caller. */
	static void Raise(const FString& Message);

	/** Raises and returns false when the folder and name cannot receive a new asset. */
	static bool CheckNewAssetPath(const FString& FolderPath, const FString& AssetName);

	/** Raises and returns false when the bone is not in the rig's skeleton. */
	static bool CheckRigBone(const UIKRigDefinition* IKRig, FName BoneName);

	/** Raises and returns false when the rig has no chain with this name. */
	static bool CheckRigChain(const UIKRigDefinition* IKRig, FName ChainName);
};
