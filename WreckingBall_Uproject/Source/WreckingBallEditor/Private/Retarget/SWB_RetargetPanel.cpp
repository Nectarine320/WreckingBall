// Fill out your copyright notice in the Description page of Project Settings.

#include "Retarget/SWB_RetargetPanel.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "PropertyCustomizationHelpers.h"
#include "Retarget/WB_RetargetProfile.h"
#include "Retarget/WB_RetargetToolset.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WBRetargetPanel"

namespace WBRetargetPanel
{
	template <typename T>
	TArray<T*> LoadAll()
	{
		TArray<FAssetData> Assets;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByClass(T::StaticClass()->GetClassPathName(), Assets);
		TArray<T*> Result;
		for (const FAssetData& Asset : Assets)
		{
			if (T* Object = Cast<T>(Asset.GetAsset()))
			{
				Result.Add(Object);
			}
		}
		return Result;
	}

	FString PathOf(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	/** "SKM_Soccer_Boy_A" -> "Soccer_Boy_A", "IK_Soccer" -> "Soccer". */
	FString ShortName(const UObject* Object)
	{
		FString Name = Object ? Object->GetName() : FString();
		for (const TCHAR* Prefix : { TEXT("SKM_"), TEXT("SK_"), TEXT("IK_") })
		{
			Name.RemoveFromStart(Prefix);
		}
		return Name;
	}

	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")));
	}

	TSharedRef<SWidget> Label(const FText& Text)
	{
		return SNew(SBox).WidthOverride(130.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text)];
	}

	/** Height of a bone in the reference pose, compared to the mesh: a retarget root on the ground breaks the pelvis height ratio. */
	float BoneHeightRatio(const USkeletalMesh* Mesh, FName Bone)
	{
		if (!Mesh || Bone.IsNone())
		{
			return 1.0f;
		}
		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
		const int32 Index = RefSkeleton.FindBoneIndex(Bone);
		const float MeshHeight = Mesh->GetImportedBounds().BoxExtent.Z * 2.0f;
		if (Index == INDEX_NONE || MeshHeight <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}
		return FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index).GetLocation().Z / MeshHeight;
	}
}

void SWB_RetargetPanel::Construct(const FArguments& InArgs)
{
	using namespace WBRetargetPanel;

	auto MeshPicker = [this](bool bSource)
	{
		return SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.ObjectPath_Lambda([this, bSource]() { return PathOf((bSource ? SourceMesh : TargetMesh).Get()); })
			.OnObjectChanged_Lambda([this, bSource](const FAssetData& Asset) { SetMesh(bSource, Cast<USkeletalMesh>(Asset.GetAsset())); })
			.DisplayThumbnail(false);
	};
	auto RigRow = [this](bool bSource)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("IKRig", "  IK Rig"))]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this, bSource]() { return RigStatus(bSource); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CreateRig", "Create automatically"))
				.ToolTipText(LOCTEXT("CreateRigTip", "Creates the IK Rig next to the mesh (Retarget folder) and generates its chains. Check them in step 2."))
				.Visibility_Lambda([this, bSource]() { return (bSource ? SourceMesh : TargetMesh).IsValid() && !(bSource ? SourceRig : TargetRig).IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked(this, &SWB_RetargetPanel::CreateRig, bSource)
			];
	};
	auto RootRow = [this](bool bSource)
	{
		return SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(WBToolsStyle::Warning)
			.Text_Lambda([this, bSource]() { return RootWarning(bSource); })
			.Visibility_Lambda([this, bSource]() { return RootWarning(bSource).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; });
	};
	auto TextField = [](FString& Value, const FText& Hint)
	{
		return SNew(SEditableTextBox)
			.Text_Lambda([&Value]() { return FText::FromString(Value); })
			.HintText(Hint)
			.OnTextChanged_Lambda([&Value](const FText& Text) { Value = Text.ToString(); });
	};

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8.0f)
		[
			SNew(SVerticalBox)
			// ---------------------------------------------------------------- 1 Characters
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[Heading(LOCTEXT("Step1", "Characters"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Source", "Source (copy from)"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MeshPicker(true)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[RigRow(true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Target", "Target (copy to)"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[MeshPicker(false)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[RigRow(false)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Retargeter", "Retargeter"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UIKRetargeter::StaticClass())
					.ObjectPath_Lambda([this]() { return PathOf(Retargeter.Get()); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset) { Retargeter = Cast<UIKRetargeter>(Asset.GetAsset()); RebuildMapping(); })
					.DisplayThumbnail(false)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateRetargeter", "Create"))
					.ToolTipText(LOCTEXT("CreateRetargeterTip", "Creates a retargeter between the two IK Rigs (RTG_<Source>_To_<Target>)."))
					.Visibility_Lambda([this]() { return SourceRig.IsValid() && TargetRig.IsValid() && !Retargeter.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnClicked(this, &SWB_RetargetPanel::CreateRetargeter)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenEditor", "Open editor"))
					.ToolTipText(LOCTEXT("OpenEditorTip", "Opens the Unreal retargeter editor: visual preview of the two characters and every advanced setting."))
					.IsEnabled_Lambda([this]() { return Retargeter.IsValid(); })
					.OnClicked_Lambda([this]()
					{
						if (GEditor && Retargeter.IsValid())
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Retargeter.Get());
						}
						return FReply::Handled();
					})
				]
			]
			// ---------------------------------------------------------------- 2 Mapping
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)[SNew(SSeparator).Thickness(2.0f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 6.0f)[Heading(LOCTEXT("Step2", "Mapping (target <- source)"))]
			+ SVerticalBox::Slot().AutoHeight()[RootRow(true)]
			+ SVerticalBox::Slot().AutoHeight()[RootRow(false)]
			+ SVerticalBox::Slot().AutoHeight()[SAssignNew(MappingBox, SVerticalBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Pose", "Target pose"))]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AutoAlign", "Align automatically"))
					.ToolTipText(LOCTEXT("AutoAlignTip", "Aligns the target retarget pose on the source (chain to chain). Use it when one character is in T-pose and the other in A-pose. Check with Open editor."))
					.IsEnabled_Lambda([this]() { return Retargeter.IsValid(); })
					.OnClicked_Lambda([this]() { UWB_RetargetToolset::AutoAlignRetargetPose(Retargeter.Get(), ERetargetSourceOrTarget::Target, ERetargetAutoAlignMethod::ChainToChain, {}); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ResetPose", "Reset"))
					.ToolTipText(LOCTEXT("ResetPoseTip", "Puts the target retarget pose back to the reference pose of the mesh."))
					.IsEnabled_Lambda([this]() { return Retargeter.IsValid(); })
					.OnClicked_Lambda([this]() { UWB_RetargetToolset::ResetRetargetPose(Retargeter.Get(), ERetargetSourceOrTarget::Target, {}); return FReply::Handled(); })
				]
			]
			// ---------------------------------------------------------------- 3 Animations
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)[SNew(SSeparator).Thickness(2.0f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 6.0f)[Heading(LOCTEXT("Step3", "Animations"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AddSelected", "Add the Content Browser selection"))
					.ToolTipText(LOCTEXT("AddSelectedTip", "Adds the Anim Sequences, Montages, Blend Spaces and Animation Blueprints selected in the Content Browser."))
					.OnClicked(this, &SWB_RetargetPanel::AddSelectedAnimations)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Clear", "Clear"))
					.OnClicked_Lambda([this]() { Anims.Reset(); AnimList->RequestListRefresh(); return FReply::Handled(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(220.0f).Padding(0.0f, 4.0f)
			[
				SAssignNew(AnimList, SListView<TSharedPtr<FWB_RetargetAnimItem>>)
				.ListItemsSource(&Anims)
				.OnGenerateRow(this, &SWB_RetargetPanel::OnGenerateAnimRow)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Rename", "Rename"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[TextField(Search, LOCTEXT("Search", "text to replace (e.g. SoccerPlayer)"))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("->")))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[TextField(Replace, LOCTEXT("Replace", "new text (e.g. SoccerCharacter)"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("PrefixSuffix", "Prefix / suffix"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[TextField(Prefix, LOCTEXT("Prefix", "prefix (optional)"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f)[SNullWidget::NullWidget]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[TextField(Suffix, LOCTEXT("Suffix", "suffix (optional)"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Output", "Output folder"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[TextField(OutputFolder, LOCTEXT("OutputHint", "/Game/Art/Character/<Character>/Animation"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseCBFolder", "Use Content Browser folder"))
					.OnClicked_Lambda([this]()
					{
						OutputFolder = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetCurrentPath().GetInternalPathString();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bIncludeReferenced ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bIncludeReferenced = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text(LOCTEXT("IncludeRefs", "Also retarget the animations used by the Blend Spaces and Animation Blueprints"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bOverwrite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bOverwrite = State == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(LOCTEXT("Overwrite", "Overwrite existing assets"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(WBToolsStyle::Warning)
					.Visibility_Lambda([this]() { return bOverwrite ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("OverwriteWarn", "erases manual touch-ups: the profile of step 4 redoes them. An Animation Blueprint is never overwritten (a copy ending with 1 is made)."))
				]
			]
			// ---------------------------------------------------------------- 4 Run
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)[SNew(SSeparator).Thickness(2.0f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 6.0f)[Heading(LOCTEXT("Step4", "Run"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Profile", "Touch-up profile"))]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UWB_RetargetProfile::StaticClass())
					.ObjectPath_Lambda([this]() { return PathOf(Profile.Get()); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset) { Profile = Cast<UWB_RetargetProfile>(Asset.GetAsset()); })
					.DisplayThumbnail(false)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(130.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsEnabled_Lambda([this]() { return Profile.IsValid(); })
				.IsChecked_Lambda([this]() { return bApplyProfile ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bApplyProfile = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return Profile.IsValid()
							? FText::Format(LOCTEXT("ProfileApply", "Apply the profile after the retarget: {0} notify bone rename(s), {1} rotation fix(es)"), Profile->NotifyBoneRenames.Num(), Profile->RotationFixes.Num())
							: LOCTEXT("NoProfile", "No profile for this target: no touch-up (create a Retarget Profile Data Asset to add some)");
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Run", "Run the retarget"))
				.IsEnabled(this, &SWB_RetargetPanel::CanRun)
				.OnClicked(this, &SWB_RetargetPanel::Run)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(ReportText, STextBlock).AutoWrapText(true)
			]
		]
	];
	RebuildMapping();
}

// ======================================================================== Step 1

void SWB_RetargetPanel::SetMesh(bool bSource, USkeletalMesh* Mesh)
{
	(bSource ? SourceMesh : TargetMesh) = Mesh;
	if (!bSource)
	{
		Profile = UWB_RetargetProfile::FindForMesh(Mesh);
	}
	if (!bSource && Mesh && OutputFolder.IsEmpty())
	{
		// Default output: next to the target character.
		OutputFolder = FPackageName::GetLongPackagePath(Mesh->GetPackage()->GetName());
	}
	DetectRigsAndRetargeter();
}

void SWB_RetargetPanel::DetectRigsAndRetargeter()
{
	using namespace WBRetargetPanel;

	SourceRig = nullptr;
	TargetRig = nullptr;
	for (UIKRigDefinition* Rig : LoadAll<UIKRigDefinition>())
	{
		if (SourceMesh.IsValid() && !SourceRig.IsValid() && Rig->GetPreviewMesh() == SourceMesh.Get())
		{
			SourceRig = Rig;
		}
		if (TargetMesh.IsValid() && !TargetRig.IsValid() && Rig->GetPreviewMesh() == TargetMesh.Get())
		{
			TargetRig = Rig;
		}
	}
	Retargeter = nullptr;
	if (SourceRig.IsValid() && TargetRig.IsValid())
	{
		for (UIKRetargeter* Candidate : LoadAll<UIKRetargeter>())
		{
			if (Candidate->GetIKRig(ERetargetSourceOrTarget::Source) == SourceRig.Get() && Candidate->GetIKRig(ERetargetSourceOrTarget::Target) == TargetRig.Get())
			{
				Retargeter = Candidate;
				break;
			}
		}
	}
	RebuildMapping();
}

FReply SWB_RetargetPanel::CreateRig(bool bSource)
{
	using namespace WBRetargetPanel;

	USkeletalMesh* Mesh = (bSource ? SourceMesh : TargetMesh).Get();
	if (!Mesh)
	{
		return FReply::Handled();
	}
	const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetPackage()->GetName()) / TEXT("Retarget");
	UIKRigDefinition* Rig = UWB_RetargetToolset::CreateIKRig(Folder, TEXT("IK_") + ShortName(Mesh), Mesh);
	if (Rig)
	{
		UWB_RetargetToolset::AutoGenerateRetargetChains(Rig);
		ReportText->SetText(FText::Format(LOCTEXT("RigCreated", "{0} created in {1} (not saved yet). Check its retarget root and chains in step 2."), FText::FromString(Rig->GetName()), FText::FromString(Folder)));
	}
	else
	{
		ReportText->SetText(LOCTEXT("RigFailed", "The IK Rig could not be created: see the Output Log."));
	}
	DetectRigsAndRetargeter();
	return FReply::Handled();
}

FReply SWB_RetargetPanel::CreateRetargeter()
{
	using namespace WBRetargetPanel;

	if (!SourceRig.IsValid() || !TargetRig.IsValid())
	{
		return FReply::Handled();
	}
	const FString Folder = FPackageName::GetLongPackagePath(TargetRig->GetPackage()->GetName());
	const FString Name = FString::Printf(TEXT("RTG_%s_To_%s"), *ShortName(SourceRig.Get()), *ShortName(TargetRig.Get()));
	Retargeter = UWB_RetargetToolset::CreateRetargeter(Folder, Name, SourceRig.Get(), TargetRig.Get());
	ReportText->SetText(Retargeter.IsValid()
		? FText::Format(LOCTEXT("RtgCreated", "{0} created in {1} (not saved yet). Check the mapping and the pose in step 2."), FText::FromString(Name), FText::FromString(Folder))
		: LOCTEXT("RtgFailed", "The retargeter could not be created: see the Output Log."));
	RebuildMapping();
	return FReply::Handled();
}

FText SWB_RetargetPanel::RigStatus(bool bSource) const
{
	const UIKRigDefinition* Rig = (bSource ? SourceRig : TargetRig).Get();
	if (!(bSource ? SourceMesh : TargetMesh).IsValid())
	{
		return LOCTEXT("PickMesh", "pick a skeletal mesh");
	}
	return Rig ? FText::Format(LOCTEXT("RigFound", "{0}  (found)"), FText::FromString(Rig->GetName())) : LOCTEXT("NoRig", "none for this mesh");
}

FText SWB_RetargetPanel::RootWarning(bool bSource) const
{
	UIKRigDefinition* Rig = (bSource ? SourceRig : TargetRig).Get();
	const USkeletalMesh* Mesh = (bSource ? SourceMesh : TargetMesh).Get();
	if (!Rig || !Mesh)
	{
		return FText::GetEmpty();
	}
	const FName Root = Rig->GetPelvis();
	if (Root.IsNone())
	{
		return FText::Format(LOCTEXT("NoRoot", "{0}: no retarget root. Pick the pelvis bone below."), FText::FromString(Rig->GetName()));
	}
	if (WBRetargetPanel::BoneHeightRatio(Mesh, Root) < 0.25f)
	{
		return FText::Format(LOCTEXT("LowRoot", "{0}: the retarget root {1} is near the ground. Pick the bone at hip height (pelvis), or the character height ratio will be wrong."), FText::FromString(Rig->GetName()), FText::FromName(Root));
	}
	return FText::GetEmpty();
}

// ======================================================================== Step 2

void SWB_RetargetPanel::RebuildMapping()
{
	using namespace WBRetargetPanel;

	MappingBox->ClearChildren();

	// Retarget roots of both rigs: a combo of the mesh bones.
	for (const bool bSource : { true, false })
	{
		UIKRigDefinition* Rig = (bSource ? SourceRig : TargetRig).Get();
		const USkeletalMesh* Mesh = (bSource ? SourceMesh : TargetMesh).Get();
		if (!Rig || !Mesh)
		{
			continue;
		}
		TSharedRef<TArray<TSharedPtr<FName>>> Bones = MakeShared<TArray<TSharedPtr<FName>>>();
		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
		for (int32 Index = 0; Index < RefSkeleton.GetNum(); ++Index)
		{
			Bones->Add(MakeShared<FName>(RefSkeleton.GetBoneName(Index)));
		}
		TWeakObjectPtr<UIKRigDefinition> WeakRig = Rig;
		MappingBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(bSource ? LOCTEXT("SourceRoot", "Source pelvis") : LOCTEXT("TargetRoot", "Target pelvis"))]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&Bones.Get())
				.ToolTipText(LOCTEXT("RootTip", "Retarget root: the pelvis bone, at hip height. Its motion moves the whole body."))
				.OnGenerateWidget_Lambda([Bones](TSharedPtr<FName> Bone) { return SNew(STextBlock).Text(FText::FromName(*Bone)); })
				.OnSelectionChanged_Lambda([WeakRig](TSharedPtr<FName> Bone, ESelectInfo::Type Type)
				{
					if (Bone && WeakRig.IsValid() && Type != ESelectInfo::Direct)
					{
						UWB_RetargetToolset::SetRetargetRoot(WeakRig.Get(), *Bone);
					}
				})
				[
					SNew(STextBlock).Text_Lambda([WeakRig]() { return WeakRig.IsValid() ? FText::FromName(WeakRig->GetPelvis()) : FText::GetEmpty(); })
				]
			]
		];
	}

	UIKRetargeter* Current = Retargeter.Get();
	if (!Current || !SourceRig.IsValid())
	{
		MappingBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock).ColorAndOpacity(WBToolsStyle::Dim).Text(LOCTEXT("NoMapping", "Pick the two characters and a retargeter to see the chain mapping."))
		];
		return;
	}

	// Chain mapping: one row per target chain, a combo of the source chains.
	TSharedRef<TArray<TSharedPtr<FName>>> SourceChains = MakeShared<TArray<TSharedPtr<FName>>>();
	SourceChains->Add(MakeShared<FName>(NAME_None));
	for (const FWB_RetargetChainInfo& Chain : UWB_RetargetToolset::GetRetargetDefinition(SourceRig.Get()).Chains)
	{
		SourceChains->Add(MakeShared<FName>(Chain.ChainName));
	}
	TWeakObjectPtr<UIKRetargeter> WeakRetargeter = Current;
	for (const FWB_RetargetChainPair& Pair : UWB_RetargetToolset::GetChainMapping(Current))
	{
		const FName TargetChain = Pair.TargetChain;
		TSharedRef<FName> Selected = MakeShared<FName>(Pair.SourceChain);
		MappingBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(FText::FromName(TargetChain))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("<-")))]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&SourceChains.Get())
				.OnGenerateWidget_Lambda([SourceChains](TSharedPtr<FName> Chain) { return SNew(STextBlock).Text(Chain->IsNone() ? LOCTEXT("NotAnimated", "(not animated)") : FText::FromName(*Chain)); })
				.OnSelectionChanged_Lambda([WeakRetargeter, TargetChain, Selected](TSharedPtr<FName> Chain, ESelectInfo::Type Type)
				{
					if (Chain && WeakRetargeter.IsValid() && Type != ESelectInfo::Direct)
					{
						UWB_RetargetToolset::SetChainMapping(WeakRetargeter.Get(), TargetChain, *Chain);
						*Selected = *Chain;
					}
				})
				[
					SNew(STextBlock).Text_Lambda([Selected]() { return Selected->IsNone() ? LOCTEXT("NotAnimatedShort", "(not animated)") : FText::FromName(*Selected); })
				]
			]
		];
	}
}

// ======================================================================== Step 3

FReply SWB_RetargetPanel::AddSelectedAnimations()
{
	TArray<FAssetData> Selected;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
	int32 Added = 0;
	for (const FAssetData& Asset : Selected)
	{
		UObject* Object = Asset.GetAsset();
		const bool bAnim = Object && (Object->IsA<UAnimSequenceBase>() || Object->IsA<UBlendSpace>() || Object->IsA<UAnimBlueprint>());
		const bool bKnown = Anims.ContainsByPredicate([Object](const TSharedPtr<FWB_RetargetAnimItem>& Item) { return Item->Asset.Get() == Object; });
		if (bAnim && !bKnown)
		{
			TSharedPtr<FWB_RetargetAnimItem> Item = MakeShared<FWB_RetargetAnimItem>();
			Item->Asset = Object;
			Anims.Add(Item);
			++Added;
		}
	}
	AnimList->RequestListRefresh();
	ReportText->SetText(FText::Format(LOCTEXT("Added", "{0} animation asset(s) added ({1} selected)."), Added, Selected.Num()));
	return FReply::Handled();
}

TSharedRef<ITableRow> SWB_RetargetPanel::OnGenerateAnimRow(TSharedPtr<FWB_RetargetAnimItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const UObject* Asset = Item->Asset.Get();
	return SNew(STableRow<TSharedPtr<FWB_RetargetAnimItem>>, OwnerTable)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Item]() { return Item->bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Item](ECheckBoxState State) { Item->bChecked = State == ECheckBoxState::Checked; })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Asset ? FString::Printf(TEXT("%s   (%s)"), *Asset->GetName(), *Asset->GetClass()->GetName()) : TEXT("(deleted)")))
				.ToolTipText(FText::FromString(Asset ? Asset->GetPathName() : FString()))
			]
		];
}

// ======================================================================== Step 4

bool SWB_RetargetPanel::CanRun() const
{
	const bool bAnyChecked = Anims.ContainsByPredicate([](const TSharedPtr<FWB_RetargetAnimItem>& Item) { return Item->bChecked && Item->Asset.IsValid(); });
	return Retargeter.IsValid() && bAnyChecked && FPackageName::IsValidPath(OutputFolder);
}

FReply SWB_RetargetPanel::Run()
{
	TArray<UObject*> Assets;
	for (const TSharedPtr<FWB_RetargetAnimItem>& Item : Anims)
	{
		if (Item->bChecked && Item->Asset.IsValid())
		{
			Assets.Add(Item->Asset.Get());
		}
	}
	if (bOverwrite && FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ConfirmOverwrite", "Overwrite the existing retargeted assets? Their manual touch-ups are erased; the touch-up profile redoes the ones it knows.")) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	const TArray<UObject*> Created = UWB_RetargetToolset::RetargetAnimations(Retargeter.Get(), Assets, OutputFolder, Search, Replace, Prefix, Suffix, bIncludeReferenced, bOverwrite);

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Retarget done: %d asset(s) created in %s (not saved yet: File > Save All)."), Created.Num(), *OutputFolder));
	if (Created.Num() == 0)
	{
		Lines.Add(TEXT("Nothing was created: see the Output Log."));
	}
	if (Profile.IsValid() && bApplyProfile && Created.Num() > 0)
	{
		TArray<FString> Log;
		const int32 Changes = Profile->ApplyFixes(Created, Log);
		Lines.Add(FString::Printf(TEXT("Profile %s: %d touch-up(s)."), *Profile->GetName(), Changes));
		for (const FString& Line : Log)
		{
			Lines.Add(TEXT("  ") + Line);
		}
	}
	for (const UObject* Asset : Created)
	{
		if (Asset && Asset->IsA<UAnimBlueprint>() && bOverwrite)
		{
			Lines.Add(FString::Printf(TEXT("  Check %s: an Animation Blueprint is never overwritten, delete the old one if this is a copy."), *Asset->GetName()));
		}
	}
	ReportText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
