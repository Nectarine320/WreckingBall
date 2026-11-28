// Fill out your copyright notice in the Description page of Project Settings.

#include "Retarget/WB_RetargetProfile.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Retarget/WB_RetargetToolset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WB_RetargetProfile)

namespace WBRetargetProfile
{
	/** Replaces the bone names held by FName properties of a notify object. Returns the number of replacements. */
	int32 RenameBones(UObject* Notify, const TArray<FWB_NotifyBoneRename>& Renames, const FString& AnimName, TArray<FString>& OutLog)
	{
		int32 Count = 0;
		if (!Notify)
		{
			return Count;
		}
		for (TFieldIterator<FNameProperty> It(Notify->GetClass()); It; ++It)
		{
			FName* Value = It->ContainerPtrToValuePtr<FName>(Notify);
			for (const FWB_NotifyBoneRename& Rename : Renames)
			{
				if (!Rename.From.IsNone() && Value->IsEqual(Rename.From, ENameCase::IgnoreCase))
				{
					Notify->Modify();
					*Value = Rename.To;
					OutLog.Add(FString::Printf(TEXT("%s: %s.%s %s -> %s"), *AnimName, *Notify->GetClass()->GetName(), *It->GetName(), *Rename.From.ToString(), *Rename.To.ToString()));
					++Count;
					break;
				}
			}
		}
		return Count;
	}
}

int32 UWB_RetargetProfile::ApplyFixes(const TArray<UObject*>& Assets, TArray<FString>& OutLog) const
{
	int32 Count = 0;
	for (UObject* Asset : Assets)
	{
		UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Asset);
		if (!Anim)
		{
			continue;
		}
		const FString AnimName = Anim->GetName();
		bool bChanged = false;

		// Notify bone names.
		if (NotifyBoneRenames.Num() > 0)
		{
			for (FAnimNotifyEvent& Event : Anim->Notifies)
			{
				const int32 Renamed = WBRetargetProfile::RenameBones(Event.Notify, NotifyBoneRenames, AnimName, OutLog)
					+ WBRetargetProfile::RenameBones(Event.NotifyStateClass, NotifyBoneRenames, AnimName, OutLog);
				Count += Renamed;
				bChanged |= Renamed > 0;
			}
		}

		// Bone rotations, on Anim Sequences only.
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(Anim))
		{
			for (const FWB_BoneRotationFix& Fix : RotationFixes)
			{
				if (Fix.AnimNameContains.IsEmpty() || !AnimName.Contains(Fix.AnimNameContains))
				{
					continue;
				}
				if (Sequence->GetSkeleton() && Sequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Fix.Bone) == INDEX_NONE)
				{
					OutLog.Add(FString::Printf(TEXT("WARNING %s: bone %s not in the skeleton, rotation fix skipped."), *AnimName, *Fix.Bone.ToString()));
					continue;
				}
				UWB_RetargetToolset::OffsetBoneRotation(Sequence, Fix.Bone, Fix.Offset, Fix.StartFrame, Fix.EndFrame, Fix.BlendInFrames, Fix.BlendOutFrames);
				OutLog.Add(FString::Printf(TEXT("%s: %s rotated by %s (frames %d to %d, blend %d / %d)"), *AnimName, *Fix.Bone.ToString(), *Fix.Offset.ToString(),
					Fix.StartFrame, Fix.EndFrame, Fix.BlendInFrames, Fix.BlendOutFrames));
				++Count;
				bChanged = true;
			}
		}
		if (bChanged)
		{
			Anim->MarkPackageDirty();
		}
	}
	return Count;
}

UWB_RetargetProfile* UWB_RetargetProfile::FindForMesh(const USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByClass(UWB_RetargetProfile::StaticClass()->GetClassPathName(), Assets);
	for (const FAssetData& Asset : Assets)
	{
		UWB_RetargetProfile* Profile = Cast<UWB_RetargetProfile>(Asset.GetAsset());
		if (Profile && Profile->TargetMesh.ToSoftObjectPath() == FSoftObjectPath(Mesh))
		{
			return Profile;
		}
	}
	return nullptr;
}
