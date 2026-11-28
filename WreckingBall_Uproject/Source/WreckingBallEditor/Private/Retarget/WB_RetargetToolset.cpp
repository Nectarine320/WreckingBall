// Fill out your copyright notice in the Description page of Project Settings.

#include "Retarget/WB_RetargetToolset.h"
#include "Retarget/WB_RetargetProfile.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationAsset.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "IAssetTools.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargetFactory.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetChainMapping.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "RigEditor/IKRigDefinitionFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_RetargetToolset)

// ==================================================================== Helpers

void UWB_RetargetToolset::Raise(const FString& Message)
{
	UKismetSystemLibrary::RaiseScriptError(Message);
}

bool UWB_RetargetToolset::CheckNewAssetPath(const FString& FolderPath, const FString& AssetName)
{
	if (AssetName.IsEmpty())
	{
		Raise(TEXT("AssetName must not be empty."));
		return false;
	}

	const FString PackageName = FolderPath / AssetName;
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(PackageName, /*bIncludeReadOnlyRoots=*/false, &Reason))
	{
		Raise(FString::Printf(TEXT("'%s' is not a valid asset path: %s"), *PackageName, *Reason.ToString()));
		return false;
	}

	if (UEditorAssetLibrary::DoesAssetExist(PackageName))
	{
		Raise(FString::Printf(TEXT("An asset already exists at '%s'."), *PackageName));
		return false;
	}
	return true;
}

bool UWB_RetargetToolset::CheckRigBone(const UIKRigDefinition* IKRig, FName BoneName)
{
	if (IKRig->GetSkeleton().GetBoneIndexFromName(BoneName) == INDEX_NONE)
	{
		Raise(FString::Printf(TEXT("Bone '%s' is not in the skeleton of IK Rig '%s'."), *BoneName.ToString(), *IKRig->GetName()));
		return false;
	}
	return true;
}

bool UWB_RetargetToolset::CheckRigChain(const UIKRigDefinition* IKRig, FName ChainName)
{
	if (!IKRig->GetRetargetChainByName(ChainName))
	{
		Raise(FString::Printf(TEXT("IK Rig '%s' has no retarget chain named '%s'."), *IKRig->GetName(), *ChainName.ToString()));
		return false;
	}
	return true;
}

namespace WB_RetargetToolset
{
	/** Index of an op in the retargeter's stack, or INDEX_NONE. */
	int32 FindOpIndex(const UIKRetargeterController* Controller, const FIKRetargetOpBase* Op)
	{
		for (int32 OpIndex = 0; OpIndex < Controller->GetNumRetargetOps(); ++OpIndex)
		{
			if (Controller->GetRetargetOpByIndex(OpIndex) == Op)
			{
				return OpIndex;
			}
		}
		return INDEX_NONE;
	}

	/**
	 * The retarget processor strips bone scale, so a bone whose reference pose is scaled (a Blender export often
	 * scales its root bone by 100) comes out at scale 1 and the mesh shrinks. Puts the target reference-pose scale back.
	 */
	void RestoreRefPoseScale(UAnimSequence* Sequence, const USkeletalMesh* TargetMesh)
	{
		const FReferenceSkeleton& RefSkeleton = TargetMesh->GetRefSkeleton();
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		IAnimationDataController& Controller = Sequence->GetController();
		const int32 NumKeys = Model->GetNumberOfKeys();

		IAnimationDataController::FScopedBracket Bracket(Controller, NSLOCTEXT("WB_RetargetToolset", "RestoreRefPoseScale", "Restore reference-pose bone scale"), false);
		for (int32 BoneIndex = 0; BoneIndex < RefPose.Num(); ++BoneIndex)
		{
			const FVector RefScale = RefPose[BoneIndex].GetScale3D();
			if (RefScale.Equals(FVector::OneVector))
			{
				continue;
			}

			const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
			TArray<FVector> Positions;
			TArray<FQuat> Rotations;
			TArray<FVector> Scales;
			if (Model->IsValidBoneTrackName(BoneName))
			{
				TArray<FTransform> Keys;
				Model->GetBoneTrackTransforms(BoneName, Keys);
				for (const FTransform& Key : Keys)
				{
					Positions.Add(Key.GetLocation());
					Rotations.Add(Key.GetRotation());
					Scales.Add(RefScale);
				}
			}
			else
			{
				Controller.AddBoneCurve(BoneName, false);
				Positions.Init(RefPose[BoneIndex].GetLocation(), NumKeys);
				Rotations.Init(RefPose[BoneIndex].GetRotation(), NumKeys);
				Scales.Init(RefScale, NumKeys);
			}
			Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false);
		}
	}

	/** The retargeter's IK Rig on the given side, or null. */
	const UIKRigDefinition* GetRig(const UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side)
	{
		return UIKRetargeterController::GetController(Retargeter)->GetIKRig(Side);
	}
}

// ==================================================================== IK Rig

UIKRigDefinition* UWB_RetargetToolset::CreateIKRig(const FString& FolderPath, const FString& AssetName, USkeletalMesh* SkeletalMesh)
{
	if (!SkeletalMesh)
	{
		Raise(TEXT("SkeletalMesh must not be null."));
		return nullptr;
	}
	if (!CheckNewAssetPath(FolderPath, AssetName))
	{
		return nullptr;
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UIKRigDefinitionFactory* Factory = NewObject<UIKRigDefinitionFactory>();
	UIKRigDefinition* IKRig = Cast<UIKRigDefinition>(AssetTools.CreateAsset(AssetName, FolderPath, UIKRigDefinition::StaticClass(), Factory));
	if (!IKRig)
	{
		Raise(FString::Printf(TEXT("Could not create IK Rig '%s' in '%s'."), *AssetName, *FolderPath));
		return nullptr;
	}

	if (!UIKRigController::GetController(IKRig)->SetSkeletalMesh(SkeletalMesh))
	{
		Raise(FString::Printf(TEXT("IK Rig '%s' was created but rejected mesh '%s'. See the output log."), *AssetName, *SkeletalMesh->GetName()));
		return nullptr;
	}
	return IKRig;
}

FWB_IKRigRetargetInfo UWB_RetargetToolset::AutoGenerateRetargetChains(UIKRigDefinition* IKRig)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return {};
	}
	if (!IKRig->GetPreviewMesh())
	{
		Raise(FString::Printf(TEXT("IK Rig '%s' has no skeletal mesh."), *IKRig->GetName()));
		return {};
	}

	if (!UIKRigController::GetController(IKRig)->ApplyAutoGeneratedRetargetDefinition())
	{
		Raise(FString::Printf(
			TEXT("The skeleton of IK Rig '%s' matches no known template. Use SetRetargetRoot and AddRetargetChain instead."),
			*IKRig->GetName()));
		return {};
	}
	return GetRetargetDefinition(IKRig);
}

FWB_IKRigRetargetInfo UWB_RetargetToolset::GetRetargetDefinition(UIKRigDefinition* IKRig)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return {};
	}

	FWB_IKRigRetargetInfo Info;
	Info.RetargetRoot = IKRig->GetPelvis();
	for (const FBoneChain& Chain : IKRig->GetRetargetChains())
	{
		FWB_RetargetChainInfo& ChainInfo = Info.Chains.AddDefaulted_GetRef();
		ChainInfo.ChainName = Chain.ChainName;
		ChainInfo.StartBone = Chain.StartBone.BoneName;
		ChainInfo.EndBone = Chain.EndBone.BoneName;
		ChainInfo.Goal = Chain.IKGoalName;
	}
	return Info;
}

void UWB_RetargetToolset::SetRetargetRoot(UIKRigDefinition* IKRig, FName BoneName)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return;
	}
	if (!CheckRigBone(IKRig, BoneName))
	{
		return;
	}

	if (!UIKRigController::GetController(IKRig)->SetRetargetRoot(BoneName))
	{
		Raise(FString::Printf(TEXT("Could not set '%s' as retarget root of '%s'."), *BoneName.ToString(), *IKRig->GetName()));
	}
}

FName UWB_RetargetToolset::AddRetargetChain(UIKRigDefinition* IKRig, FName ChainName, FName StartBone, FName EndBone)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return NAME_None;
	}
	if (ChainName.IsNone())
	{
		Raise(TEXT("ChainName must not be None."));
		return NAME_None;
	}
	if (!CheckRigBone(IKRig, StartBone) || !CheckRigBone(IKRig, EndBone))
	{
		return NAME_None;
	}

	const FName NewName = UIKRigController::GetController(IKRig)->AddRetargetChain(ChainName, StartBone, EndBone, NAME_None);
	if (NewName.IsNone())
	{
		Raise(FString::Printf(TEXT("Could not add chain '%s' (%s -> %s) to '%s'. See the output log."),
			*ChainName.ToString(), *StartBone.ToString(), *EndBone.ToString(), *IKRig->GetName()));
	}
	return NewName;
}

void UWB_RetargetToolset::SetRetargetChainBones(UIKRigDefinition* IKRig, FName ChainName, FName StartBone, FName EndBone)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return;
	}
	if (!CheckRigChain(IKRig, ChainName) || !CheckRigBone(IKRig, StartBone) || !CheckRigBone(IKRig, EndBone))
	{
		return;
	}

	const UIKRigController* Controller = UIKRigController::GetController(IKRig);
	if (!Controller->SetRetargetChainStartBone(ChainName, StartBone) || !Controller->SetRetargetChainEndBone(ChainName, EndBone))
	{
		Raise(FString::Printf(TEXT("Could not set the bones of chain '%s' in '%s'. See the output log."), *ChainName.ToString(), *IKRig->GetName()));
	}
}

void UWB_RetargetToolset::RemoveRetargetChain(UIKRigDefinition* IKRig, FName ChainName)
{
	if (!IKRig)
	{
		Raise(TEXT("IKRig must not be null."));
		return;
	}
	if (!CheckRigChain(IKRig, ChainName))
	{
		return;
	}

	if (!UIKRigController::GetController(IKRig)->RemoveRetargetChain(ChainName))
	{
		Raise(FString::Printf(TEXT("Could not remove chain '%s' from '%s'."), *ChainName.ToString(), *IKRig->GetName()));
	}
}

// ==================================================================== IK Retargeter

UIKRetargeter* UWB_RetargetToolset::CreateRetargeter(const FString& FolderPath, const FString& AssetName, UIKRigDefinition* SourceIKRig, UIKRigDefinition* TargetIKRig)
{
	if (!SourceIKRig || !TargetIKRig)
	{
		Raise(TEXT("SourceIKRig and TargetIKRig must not be null."));
		return nullptr;
	}
	for (const UIKRigDefinition* Rig : { SourceIKRig, TargetIKRig })
	{
		if (!Rig->GetPreviewMesh())
		{
			Raise(FString::Printf(TEXT("IK Rig '%s' has no skeletal mesh."), *Rig->GetName()));
			return nullptr;
		}
	}
	if (!CheckNewAssetPath(FolderPath, AssetName))
	{
		return nullptr;
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UIKRetargetFactory* Factory = NewObject<UIKRetargetFactory>();
	UIKRetargeter* Retargeter = Cast<UIKRetargeter>(AssetTools.CreateAsset(AssetName, FolderPath, UIKRetargeter::StaticClass(), Factory));
	if (!Retargeter)
	{
		Raise(FString::Printf(TEXT("Could not create IK Retargeter '%s' in '%s'."), *AssetName, *FolderPath));
		return nullptr;
	}

	// Same setup as the editor's batch retarget window (FProceduralRetargetAssets::AutoGenerateIKRetargetAsset).
	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceIKRig);
	Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetIKRig);
	Controller->RemoveAllOps();
	Controller->AddDefaultOps();
	Controller->AutoMapChains(EAutoMapChainType::Exact, /*bForceRemap=*/true);

	// The IK pass needs IK goals, which the auto setup does not make reliable: off by default.
	if (FIKRetargetRunIKRigOp* RunIKOp = Controller->GetFirstRetargetOpOfType<FIKRetargetRunIKRigOp>())
	{
		Controller->SetRetargetOpEnabled(WB_RetargetToolset::FindOpIndex(Controller, RunIKOp), false);
	}

	// Root motion: a target whose root is its pelvis lets the pelvis op move it. Otherwise, when the source
	// has no separate root bone, the target root is generated under the target pelvis.
	if (FIKRetargetRootMotionOp* RootOp = Controller->GetFirstRetargetOpOfType<FIKRetargetRootMotionOp>())
	{
		const FName TargetSkeletonRoot = TargetIKRig->GetSkeleton().BoneNames.IsEmpty() ? NAME_None : TargetIKRig->GetSkeleton().BoneNames[0];
		const FName SourceSkeletonRoot = SourceIKRig->GetSkeleton().BoneNames.IsEmpty() ? NAME_None : SourceIKRig->GetSkeleton().BoneNames[0];
		if (TargetSkeletonRoot == TargetIKRig->GetPelvis())
		{
			Controller->SetRetargetOpEnabled(WB_RetargetToolset::FindOpIndex(Controller, RootOp), false);
		}
		else if (SourceSkeletonRoot == SourceIKRig->GetPelvis())
		{
			Retargeter->Modify();
			RootOp->Settings.RootMotionSource = ERootMotionSource::GenerateFromTargetPelvis;
			RootOp->Settings.RootHeightSource = ERootMotionHeightSource::SnapToGround;
			RootOp->Settings.bRotateWithPelvis = true;
			RootOp->Settings.bMaintainOffsetFromPelvis = true;
		}
	}
	return Retargeter;
}

TArray<FWB_RetargetChainPair> UWB_RetargetToolset::GetChainMapping(UIKRetargeter* Retargeter)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return {};
	}

	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	FIKRetargetFKChainsOp* FKOp = Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>();
	const FRetargetChainMapping* Mapping = FKOp ? FKOp->GetChainMapping() : nullptr;
	if (!Mapping)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no FK chains op."), *Retargeter->GetName()));
		return {};
	}

	TArray<FWB_RetargetChainPair> Pairs;
	for (const FRetargetChainPair& Pair : Mapping->GetChainPairs())
	{
		Pairs.Add({ Pair.TargetChainName, Pair.SourceChainName });
	}
	return Pairs;
}

void UWB_RetargetToolset::SetChainMapping(UIKRetargeter* Retargeter, FName TargetChain, FName SourceChain)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	const UIKRigDefinition* SourceRig = WB_RetargetToolset::GetRig(Retargeter, ERetargetSourceOrTarget::Source);
	const UIKRigDefinition* TargetRig = WB_RetargetToolset::GetRig(Retargeter, ERetargetSourceOrTarget::Target);
	if (!SourceRig || !TargetRig)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' needs both a source and a target IK Rig."), *Retargeter->GetName()));
		return;
	}
	if (!CheckRigChain(TargetRig, TargetChain) || (!SourceChain.IsNone() && !CheckRigChain(SourceRig, SourceChain)))
	{
		return;
	}

	if (!UIKRetargeterController::GetController(Retargeter)->SetSourceChain(SourceChain, TargetChain))
	{
		Raise(FString::Printf(TEXT("No op of '%s' maps target chain '%s'."), *Retargeter->GetName(), *TargetChain.ToString()));
	}
}

namespace WB_RetargetToolset
{
	/** Controller of the retargeter's first FK chains op, or null. */
	UIKRetargetFKChainsController* GetFKChainsController(const UIKRetargeter* Retargeter)
	{
		UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
		const int32 OpIndex = FindOpIndex(Controller, Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>());
		return OpIndex == INDEX_NONE ? nullptr : Cast<UIKRetargetFKChainsController>(Controller->GetOpController(OpIndex));
	}
}

FRetargetFKChainSettings UWB_RetargetToolset::GetFKChainSettings(UIKRetargeter* Retargeter, FName TargetChain)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return {};
	}
	UIKRetargetFKChainsController* OpController = WB_RetargetToolset::GetFKChainsController(Retargeter);
	if (!OpController)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no FK chains op."), *Retargeter->GetName()));
		return {};
	}

	const FIKRetargetFKChainsOpSettings OpSettings = OpController->GetSettings();
	const FRetargetFKChainSettings* Found = OpSettings.ChainsToRetarget.FindByPredicate(
		[TargetChain](const FRetargetFKChainSettings& Chain) { return Chain.TargetChainName == TargetChain; });
	if (!Found)
	{
		Raise(FString::Printf(TEXT("The FK chains op of '%s' has no target chain named '%s'."), *Retargeter->GetName(), *TargetChain.ToString()));
		return {};
	}
	return *Found;
}

void UWB_RetargetToolset::SetFKChainSettings(UIKRetargeter* Retargeter, const FRetargetFKChainSettings& Settings)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	UIKRetargetFKChainsController* OpController = WB_RetargetToolset::GetFKChainsController(Retargeter);
	if (!OpController)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no FK chains op."), *Retargeter->GetName()));
		return;
	}

	FIKRetargetFKChainsOpSettings OpSettings = OpController->GetSettings();
	FRetargetFKChainSettings* Found = OpSettings.ChainsToRetarget.FindByPredicate(
		[&Settings](const FRetargetFKChainSettings& Chain) { return Chain.TargetChainName == Settings.TargetChainName; });
	if (!Found)
	{
		Raise(FString::Printf(TEXT("The FK chains op of '%s' has no target chain named '%s'."), *Retargeter->GetName(), *Settings.TargetChainName.ToString()));
		return;
	}

	Retargeter->Modify();
	*Found = Settings;
	OpController->SetSettings(OpSettings);
}

namespace WB_RetargetToolset
{
	/** Settings of the retargeter's first pelvis motion op, edited in place, or null. */
	FIKRetargetPelvisMotionOpSettings* GetPelvisMotionSettings(const UIKRetargeter* Retargeter)
	{
		FIKRetargetPelvisMotionOp* Op = UIKRetargeterController::GetController(Retargeter)->GetFirstRetargetOpOfType<FIKRetargetPelvisMotionOp>();
		return Op ? static_cast<FIKRetargetPelvisMotionOpSettings*>(Op->GetSettings()) : nullptr;
	}
}

FWB_PelvisTranslationScale UWB_RetargetToolset::GetPelvisTranslationScale(UIKRetargeter* Retargeter)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return {};
	}
	const FIKRetargetPelvisMotionOpSettings* Settings = WB_RetargetToolset::GetPelvisMotionSettings(Retargeter);
	if (!Settings)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no pelvis motion op."), *Retargeter->GetName()));
		return {};
	}
	FWB_PelvisTranslationScale Scale;
	Scale.Horizontal = Settings->ScaleHorizontal;
	Scale.Vertical = Settings->ScaleVertical;
	return Scale;
}

void UWB_RetargetToolset::SetPelvisTranslationScale(UIKRetargeter* Retargeter, const FWB_PelvisTranslationScale& Scale)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	FIKRetargetPelvisMotionOpSettings* Settings = WB_RetargetToolset::GetPelvisMotionSettings(Retargeter);
	if (!Settings)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no pelvis motion op."), *Retargeter->GetName()));
		return;
	}

	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	FScopedReinitializeIKRetargeter Reinitialize(Controller);
	Retargeter->Modify();
	Settings->ScaleHorizontal = Scale.Horizontal;
	Settings->ScaleVertical = Scale.Vertical;
}

void UWB_RetargetToolset::AutoAlignRetargetPose(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, ERetargetAutoAlignMethod Method, const TArray<FName>& Bones)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	const UIKRigDefinition* Rig = WB_RetargetToolset::GetRig(Retargeter, Side);
	if (!Rig)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no IK Rig on that side."), *Retargeter->GetName()));
		return;
	}
	for (const FName Bone : Bones)
	{
		if (!CheckRigBone(Rig, Bone))
		{
			return;
		}
	}

	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (Bones.IsEmpty())
	{
		Controller->AutoAlignAllBones(Side, Method);
	}
	else
	{
		Controller->AutoAlignBones(Bones, Method, Side);
	}
}

FRotator UWB_RetargetToolset::GetRetargetPoseBoneRotation(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, FName BoneName)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return FRotator::ZeroRotator;
	}
	const UIKRigDefinition* Rig = WB_RetargetToolset::GetRig(Retargeter, Side);
	if (!Rig)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no IK Rig on that side."), *Retargeter->GetName()));
		return FRotator::ZeroRotator;
	}
	if (!CheckRigBone(Rig, BoneName))
	{
		return FRotator::ZeroRotator;
	}

	return UIKRetargeterController::GetController(Retargeter)->GetRotationOffsetForRetargetPoseBone(BoneName, Side).Rotator();
}

void UWB_RetargetToolset::SetRetargetPoseBoneRotation(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, FName BoneName, FRotator Rotation)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	const UIKRigDefinition* Rig = WB_RetargetToolset::GetRig(Retargeter, Side);
	if (!Rig)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no IK Rig on that side."), *Retargeter->GetName()));
		return;
	}
	if (!CheckRigBone(Rig, BoneName))
	{
		return;
	}

	Retargeter->Modify();
	UIKRetargeterController::GetController(Retargeter)->SetRotationOffsetForRetargetPoseBone(BoneName, Rotation.Quaternion(), Side);
}

void UWB_RetargetToolset::ResetRetargetPose(UIKRetargeter* Retargeter, ERetargetSourceOrTarget Side, const TArray<FName>& Bones)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return;
	}
	const UIKRigDefinition* Rig = WB_RetargetToolset::GetRig(Retargeter, Side);
	if (!Rig)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' has no IK Rig on that side."), *Retargeter->GetName()));
		return;
	}
	for (const FName Bone : Bones)
	{
		if (!CheckRigBone(Rig, Bone))
		{
			return;
		}
	}

	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	Controller->ResetRetargetPose(Controller->GetCurrentRetargetPoseName(Side), Bones, Side);
}

// ==================================================================== Batch

TArray<UObject*> UWB_RetargetToolset::RetargetAnimations(
	UIKRetargeter* Retargeter,
	const TArray<UObject*>& Assets,
	const FString& TargetFolder,
	const FString& Search,
	const FString& Replace,
	const FString& Prefix,
	const FString& Suffix,
	bool bIncludeReferencedAssets,
	bool bOverwriteExisting)
{
	if (!Retargeter)
	{
		Raise(TEXT("Retargeter must not be null."));
		return {};
	}
	if (Assets.IsEmpty())
	{
		Raise(TEXT("Assets must not be empty."));
		return {};
	}
	if (!FPackageName::IsValidLongPackageName(TargetFolder))
	{
		Raise(FString::Printf(TEXT("'%s' is not a valid content folder."), *TargetFolder));
		return {};
	}

	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	USkeletalMesh* SourceMesh = Controller->GetPreviewMesh(ERetargetSourceOrTarget::Source);
	USkeletalMesh* TargetMesh = Controller->GetPreviewMesh(ERetargetSourceOrTarget::Target);
	if (!SourceMesh || !TargetMesh)
	{
		Raise(FString::Printf(TEXT("Retargeter '%s' needs a source and a target preview mesh."), *Retargeter->GetName()));
		return {};
	}

	FIKRetargetBatchOperationInputs Inputs;
	for (UObject* Asset : Assets)
	{
		if (!Asset || !(Asset->IsA<UAnimationAsset>() || Asset->IsA<UAnimBlueprint>()))
		{
			Raise(FString::Printf(TEXT("'%s' is not an animation asset or Animation Blueprint."), *GetNameSafe(Asset)));
			return {};
		}
		Inputs.AssetsToRetarget.Add(FAssetData(Asset));
	}
	Inputs.SourceMesh = SourceMesh;
	Inputs.TargetMesh = TargetMesh;
	Inputs.IKRetargetAsset = Retargeter;
	Inputs.Search = Search;
	Inputs.Replace = Replace;
	Inputs.Prefix = Prefix;
	Inputs.Suffix = Suffix;
	Inputs.TargetPath = TargetFolder;
	Inputs.bUseSourcePath = false;
	Inputs.bIncludeReferencedAssets = bIncludeReferencedAssets;
	Inputs.bOverwriteExistingFiles = bOverwriteExisting;

	TArray<UObject*> Created;
	for (const FAssetData& AssetData : UIKRetargetBatchOperation::RunBatchRetarget(Inputs))
	{
		if (UObject* NewAsset = AssetData.GetAsset())
		{
			if (UAnimSequence* NewSequence = Cast<UAnimSequence>(NewAsset))
			{
				WB_RetargetToolset::RestoreRefPoseScale(NewSequence, TargetMesh);
			}
			Created.Add(NewAsset);
		}
	}
	if (Created.IsEmpty())
	{
		Raise(TEXT("No asset was retargeted. See the output log (LogIKRigEditor)."));
	}
	return Created;
}

// ==================================================================== Fix

namespace WB_RetargetToolset
{
	/** Local transforms of a bone for every key: its track, or its reference pose when it has none. */
	TArray<FTransform> GetBoneKeys(const UAnimSequence* Sequence, FName BoneName, int32 RefBoneIndex)
	{
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		TArray<FTransform> Keys;
		if (Model->IsValidBoneTrackName(BoneName))
		{
			Model->GetBoneTrackTransforms(BoneName, Keys);
		}
		else
		{
			const FTransform RefPose = Sequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[RefBoneIndex];
			Keys.Init(RefPose, Model->GetNumberOfKeys());
		}
		return Keys;
	}
}

FRotator UWB_RetargetToolset::GetBoneLocalRotation(UAnimSequence* Sequence, FName BoneName, int32 Frame)
{
	if (!Sequence || !Sequence->GetSkeleton())
	{
		Raise(TEXT("Sequence must not be null and must have a skeleton."));
		return FRotator::ZeroRotator;
	}
	const int32 RefBoneIndex = Sequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (RefBoneIndex == INDEX_NONE)
	{
		Raise(FString::Printf(TEXT("Bone '%s' is not in the skeleton of '%s'."), *BoneName.ToString(), *Sequence->GetName()));
		return FRotator::ZeroRotator;
	}

	const TArray<FTransform> Keys = WB_RetargetToolset::GetBoneKeys(Sequence, BoneName, RefBoneIndex);
	if (!Keys.IsValidIndex(Frame))
	{
		Raise(FString::Printf(TEXT("Frame %d is outside '%s' (%d keys)."), Frame, *Sequence->GetName(), Keys.Num()));
		return FRotator::ZeroRotator;
	}
	return Keys[Frame].Rotator();
}

void UWB_RetargetToolset::OffsetBoneRotation(UAnimSequence* Sequence, FName BoneName, FRotator Offset, int32 StartFrame, int32 EndFrame, int32 BlendInFrames, int32 BlendOutFrames)
{
	if (!Sequence || !Sequence->GetSkeleton())
	{
		Raise(TEXT("Sequence must not be null and must have a skeleton."));
		return;
	}
	const int32 RefBoneIndex = Sequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (RefBoneIndex == INDEX_NONE)
	{
		Raise(FString::Printf(TEXT("Bone '%s' is not in the skeleton of '%s'."), *BoneName.ToString(), *Sequence->GetName()));
		return;
	}

	TArray<FTransform> Keys = WB_RetargetToolset::GetBoneKeys(Sequence, BoneName, RefBoneIndex);
	const int32 LastKey = Keys.Num() - 1;
	const int32 End = EndFrame < 0 ? LastKey : EndFrame;
	if (StartFrame < 0 || StartFrame > End || End > LastKey)
	{
		Raise(FString::Printf(TEXT("Frame range %d..%d is outside '%s' (keys 0..%d)."), StartFrame, End, *Sequence->GetName(), LastKey));
		return;
	}

	const FQuat OffsetQuat = Offset.Quaternion();
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<FVector> Scales;
	for (int32 Key = 0; Key < Keys.Num(); ++Key)
	{
		double Weight = 0.0;
		if (Key >= StartFrame && Key <= End)
		{
			Weight = 1.0;
			if (BlendInFrames > 0)
			{
				Weight = FMath::Min(Weight, double(Key - StartFrame + 1) / double(BlendInFrames + 1));
			}
			if (BlendOutFrames > 0)
			{
				Weight = FMath::Min(Weight, double(End - Key + 1) / double(BlendOutFrames + 1));
			}
		}
		const FQuat Rotation = Keys[Key].GetRotation() * FQuat::Slerp(FQuat::Identity, OffsetQuat, Weight);
		Positions.Add(Keys[Key].GetLocation());
		Rotations.Add(Rotation.GetNormalized());
		Scales.Add(Keys[Key].GetScale3D());
	}

	IAnimationDataController& Controller = Sequence->GetController();
	IAnimationDataController::FScopedBracket Bracket(Controller, NSLOCTEXT("WB_RetargetToolset", "OffsetBoneRotation", "Offset bone rotation"));
	if (!Sequence->GetDataModel()->IsValidBoneTrackName(BoneName))
	{
		Controller.AddBoneCurve(BoneName);
	}
	Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales);
}

TArray<FString> UWB_RetargetToolset::ApplyRetargetProfile(UWB_RetargetProfile* Profile, const TArray<UObject*>& Assets)
{
	TArray<FString> Log;
	if (!Profile)
	{
		Raise(TEXT("Profile must not be null."));
		return Log;
	}
	Profile->ApplyFixes(Assets, Log);
	return Log;
}
