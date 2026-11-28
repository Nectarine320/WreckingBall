// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SVerticalBox;
class STextBlock;
class UIKRetargeter;
class UIKRigDefinition;
class USkeletalMesh;
class UWB_RetargetProfile;

/** One animation asset to retarget. */
struct FWB_RetargetAnimItem
{
	TWeakObjectPtr<UObject> Asset;
	bool bChecked = true;
};

/**
 * WB Tools > Animation > Retarget: a 4-step assistant over WB_RetargetToolset. 1 Characters (meshes, IK Rigs, retargeter found or created),
 * 2 Mapping (retarget roots, chain mapping, retarget pose), 3 Animations (assets, names, output folder), 4 Run (retarget + profile touch-ups).
 */
class SWB_RetargetPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_RetargetPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Step 1
	void SetMesh(bool bSource, USkeletalMesh* Mesh);
	void DetectRigsAndRetargeter();
	FReply CreateRig(bool bSource);
	FReply CreateRetargeter();

	// Step 2
	void RebuildMapping();

	// Step 3
	FReply AddSelectedAnimations();
	TSharedRef<ITableRow> OnGenerateAnimRow(TSharedPtr<FWB_RetargetAnimItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	// Step 4
	bool CanRun() const;
	FReply Run();

	FText RigStatus(bool bSource) const;
	FText RootWarning(bool bSource) const;

	TWeakObjectPtr<USkeletalMesh> SourceMesh;
	TWeakObjectPtr<USkeletalMesh> TargetMesh;
	TWeakObjectPtr<UIKRigDefinition> SourceRig;
	TWeakObjectPtr<UIKRigDefinition> TargetRig;
	TWeakObjectPtr<UIKRetargeter> Retargeter;
	TWeakObjectPtr<UWB_RetargetProfile> Profile;

	TSharedPtr<SVerticalBox> MappingBox;
	TArray<TSharedPtr<FWB_RetargetAnimItem>> Anims;
	TSharedPtr<SListView<TSharedPtr<FWB_RetargetAnimItem>>> AnimList;
	FString Search;
	FString Replace;
	FString Prefix;
	FString Suffix;
	FString OutputFolder;
	bool bIncludeReferenced = true;
	bool bOverwrite = false;
	bool bApplyProfile = true;
	TSharedPtr<STextBlock> ReportText;
};
