// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/AnimSequence.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#include "Retarget/WB_RetargetToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	// Project content used as fixtures: the prototype character (custom 20-bone skeleton) and Soccer Boy A (Mixamo skeleton).
	const TCHAR* SourceMeshPath = TEXT("/Game/Art/Character/Prototype/SKM_Character_FirstPrototype.SKM_Character_FirstPrototype");
	const TCHAR* TargetMeshPath = TEXT("/Game/Art/Character/Soccer/Soccer_Boy_A/SKM_Soccer_Boy_A.SKM_Soccer_Boy_A");
	const TCHAR* SourceAnimPath = TEXT("/Game/Art/Character/Prototype/Animation/Dash/AS_Dash_Recoil.AS_Dash_Recoil");
	const TCHAR* TestFolder = TEXT("/Game/__WB_AutomationTests/Retarget");
}

BEGIN_DEFINE_SPEC(FWB_RetargetToolsetSpec, "WreckingBall.Toolsets.RetargetToolset", TestFlags)

	TUniquePtr<UE::ToolsetRegistry::FToolCallExceptionHandler> ExceptionHandler;
	USkeletalMesh* SourceMesh = nullptr;
	USkeletalMesh* TargetMesh = nullptr;
	bool bExpectsSourceControlWarning = false;

	// Call before a test creates assets. They are never saved, so the Git source control provider
	// warns that the test folder is missing on disk; that warning is not a failure.
	void ExpectAssetCreation()
	{
		if (!bExpectsSourceControlWarning)
		{
			AddExpectedError(TEXT("could not open directory"), EAutomationExpectedErrorFlags::Contains, 0);
			bExpectsSourceControlWarning = true;
		}
	}

	// Runs a tool call in a scope where RaiseScriptError is reported to ExceptionHandler.
	void Call(TFunction<void()>&& Func)
	{
		ExceptionHandler->CaptureErrorsIn(MoveTemp(Func));
	}

	void ExpectNoException()
	{
		TestEqual(TEXT("Error"), ExceptionHandler->GetException(), FString());
		ExceptionHandler = MakeUnique<UE::ToolsetRegistry::FToolCallExceptionHandler>();
	}

	void ExpectExceptionContains(const FString& Text)
	{
		TestTrue(*FString::Printf(TEXT("Error contains '%s'"), *Text), ExceptionHandler->GetException().Contains(Text));
		ExceptionHandler = MakeUnique<UE::ToolsetRegistry::FToolCallExceptionHandler>();
	}

	// Source rig: root Root, chains Spine (Bone -> Bone2) and LeftArm (Arm_l -> Arm2_l).
	UIKRigDefinition* MakeSourceRig()
	{
		ExpectAssetCreation();
		UIKRigDefinition* Rig = nullptr;
		Call([&]()
		{
			Rig = UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_TestSource"), SourceMesh);
			UWB_RetargetToolset::SetRetargetRoot(Rig, TEXT("Root"));
			UWB_RetargetToolset::AddRetargetChain(Rig, TEXT("Spine"), TEXT("Bone"), TEXT("Bone2"));
			UWB_RetargetToolset::AddRetargetChain(Rig, TEXT("LeftArm"), TEXT("Arm_l"), TEXT("Arm2_l"));
		});
		return Rig;
	}

	// Target rig: root Hips, chains Spine (Spine -> Spine2) and LeftArm (LeftArm -> LeftForeArm).
	UIKRigDefinition* MakeTargetRig()
	{
		ExpectAssetCreation();
		UIKRigDefinition* Rig = nullptr;
		Call([&]()
		{
			Rig = UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_TestTarget"), TargetMesh);
			UWB_RetargetToolset::SetRetargetRoot(Rig, TEXT("Hips"));
			UWB_RetargetToolset::AddRetargetChain(Rig, TEXT("Spine"), TEXT("Spine"), TEXT("Spine2"));
			UWB_RetargetToolset::AddRetargetChain(Rig, TEXT("LeftArm"), TEXT("LeftArm"), TEXT("LeftForeArm"));
		});
		return Rig;
	}

	UIKRetargeter* MakeRetargeter()
	{
		UIKRigDefinition* Source = MakeSourceRig();
		UIKRigDefinition* Target = MakeTargetRig();
		UIKRetargeter* Retargeter = nullptr;
		Call([&]() { Retargeter = UWB_RetargetToolset::CreateRetargeter(TestFolder, TEXT("RTG_Test"), Source, Target); });
		return Retargeter;
	}

END_DEFINE_SPEC(FWB_RetargetToolsetSpec)

void FWB_RetargetToolsetSpec::Define()
{
	BeforeEach([this]()
	{
		bExpectsSourceControlWarning = false;
		ExceptionHandler = MakeUnique<UE::ToolsetRegistry::FToolCallExceptionHandler>();
		SourceMesh = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
		TargetMesh = LoadObject<USkeletalMesh>(nullptr, TargetMeshPath);
		TestNotNull(TEXT("Source mesh fixture"), SourceMesh);
		TestNotNull(TEXT("Target mesh fixture"), TargetMesh);
	});

	AfterEach([this]()
	{
		ExpectNoException();
		ExceptionHandler.Reset();
		if (UEditorAssetLibrary::DoesDirectoryExist(TestFolder))
		{
			UEditorAssetLibrary::DeleteDirectory(TestFolder);
		}
	});

	Describe("CreateIKRig", [this]()
	{
		It("creates a rig bound to the mesh", [this]()
		{
			ExpectAssetCreation();
			UIKRigDefinition* Rig = nullptr;
			Call([&]() { Rig = UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_Test"), TargetMesh); });
			ExpectNoException();
			if (TestNotNull(TEXT("Rig"), Rig))
			{
				TestEqual(TEXT("Preview mesh"), Rig->GetPreviewMesh(), TargetMesh);
			}
		});

		It("raises when the mesh is null", [this]()
		{
			Call([&]() { UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_Test"), nullptr); });
			ExpectExceptionContains(TEXT("SkeletalMesh must not be null"));
		});

		It("raises when the path is invalid", [this]()
		{
			Call([&]() { UWB_RetargetToolset::CreateIKRig(TEXT("NotAPath"), TEXT("IK_Test"), TargetMesh); });
			ExpectExceptionContains(TEXT("not a valid asset path"));
		});

		It("raises when an asset already exists", [this]()
		{
			ExpectAssetCreation();
			Call([&]() { UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_Test"), TargetMesh); });
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_Test"), TargetMesh); });
			ExpectExceptionContains(TEXT("already exists"));
		});
	});

	Describe("AutoGenerateRetargetChains", [this]()
	{
		It("generates chains for a Mixamo skeleton", [this]()
		{
			ExpectAssetCreation();
			UIKRigDefinition* Rig = nullptr;
			FWB_IKRigRetargetInfo Info;
			Call([&]()
			{
				Rig = UWB_RetargetToolset::CreateIKRig(TestFolder, TEXT("IK_Test"), TargetMesh);
				Info = UWB_RetargetToolset::AutoGenerateRetargetChains(Rig);
			});
			ExpectNoException();
			TestEqual(TEXT("Retarget root"), Info.RetargetRoot, FName(TEXT("Hips")));
			TestTrue(TEXT("Has chains"), Info.Chains.Num() > 0);
		});

		It("raises when the rig is null", [this]()
		{
			Call([&]() { UWB_RetargetToolset::AutoGenerateRetargetChains(nullptr); });
			ExpectExceptionContains(TEXT("IKRig must not be null"));
		});
	});

	Describe("Retarget chains", [this]()
	{
		It("adds, edits and removes chains", [this]()
		{
			UIKRigDefinition* Rig = MakeSourceRig();
			ExpectNoException();
			FWB_IKRigRetargetInfo Info;
			Call([&]() { Info = UWB_RetargetToolset::GetRetargetDefinition(Rig); });
			TestEqual(TEXT("Root"), Info.RetargetRoot, FName(TEXT("Root")));
			TestEqual(TEXT("Chain count"), Info.Chains.Num(), 2);

			Call([&]()
			{
				UWB_RetargetToolset::SetRetargetChainBones(Rig, TEXT("Spine"), TEXT("Bone"), TEXT("Bone1"));
				Info = UWB_RetargetToolset::GetRetargetDefinition(Rig);
			});
			ExpectNoException();
			TestEqual(TEXT("New end bone"), Info.Chains[0].EndBone, FName(TEXT("Bone1")));

			Call([&]()
			{
				UWB_RetargetToolset::RemoveRetargetChain(Rig, TEXT("LeftArm"));
				Info = UWB_RetargetToolset::GetRetargetDefinition(Rig);
			});
			ExpectNoException();
			TestEqual(TEXT("Chain count after remove"), Info.Chains.Num(), 1);
		});

		It("raises on a bone that is not in the skeleton", [this]()
		{
			UIKRigDefinition* Rig = MakeSourceRig();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::AddRetargetChain(Rig, TEXT("Bad"), TEXT("NoSuchBone"), TEXT("Bone")); });
			ExpectExceptionContains(TEXT("is not in the skeleton"));
			Call([&]() { UWB_RetargetToolset::SetRetargetRoot(Rig, TEXT("NoSuchBone")); });
			ExpectExceptionContains(TEXT("is not in the skeleton"));
		});

		It("raises on a None chain name", [this]()
		{
			UIKRigDefinition* Rig = MakeSourceRig();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::AddRetargetChain(Rig, NAME_None, TEXT("Bone"), TEXT("Bone2")); });
			ExpectExceptionContains(TEXT("ChainName must not be None"));
		});

		It("raises on an unknown chain", [this]()
		{
			UIKRigDefinition* Rig = MakeSourceRig();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::RemoveRetargetChain(Rig, TEXT("NoSuchChain")); });
			ExpectExceptionContains(TEXT("has no retarget chain"));
			Call([&]() { UWB_RetargetToolset::SetRetargetChainBones(Rig, TEXT("NoSuchChain"), TEXT("Bone"), TEXT("Bone2")); });
			ExpectExceptionContains(TEXT("has no retarget chain"));
		});
	});

	Describe("Retargeter", [this]()
	{
		It("creates a retargeter with chains mapped by name", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			ExpectNoException();
			if (!TestNotNull(TEXT("Retargeter"), Retargeter))
			{
				return;
			}
			TArray<FWB_RetargetChainPair> Pairs;
			Call([&]() { Pairs = UWB_RetargetToolset::GetChainMapping(Retargeter); });
			ExpectNoException();
			TestEqual(TEXT("Pair count"), Pairs.Num(), 2);
			for (const FWB_RetargetChainPair& Pair : Pairs)
			{
				TestEqual(TEXT("Mapped by name"), Pair.SourceChain, Pair.TargetChain);
			}
		});

		It("raises when a rig is null", [this]()
		{
			Call([&]() { UWB_RetargetToolset::CreateRetargeter(TestFolder, TEXT("RTG_Test"), nullptr, nullptr); });
			ExpectExceptionContains(TEXT("must not be null"));
		});

		It("sets and clears a chain mapping", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			TArray<FWB_RetargetChainPair> Pairs;
			Call([&]()
			{
				UWB_RetargetToolset::SetChainMapping(Retargeter, TEXT("LeftArm"), NAME_None);
				Pairs = UWB_RetargetToolset::GetChainMapping(Retargeter);
			});
			ExpectNoException();
			const FWB_RetargetChainPair* Arm = Pairs.FindByPredicate([](const FWB_RetargetChainPair& P) { return P.TargetChain == TEXT("LeftArm"); });
			if (TestNotNull(TEXT("LeftArm pair"), Arm))
			{
				TestTrue(TEXT("Cleared"), Arm->SourceChain.IsNone());
			}
		});

		It("raises on an unknown chain in the mapping", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::SetChainMapping(Retargeter, TEXT("NoSuchChain"), TEXT("Spine")); });
			ExpectExceptionContains(TEXT("has no retarget chain"));
		});
	});

	Describe("Retarget pose", [this]()
	{
		It("sets, reads and resets a bone offset", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			const FRotator Wanted(0.0, 0.0, -45.0);
			FRotator Read;
			Call([&]()
			{
				UWB_RetargetToolset::SetRetargetPoseBoneRotation(Retargeter, ERetargetSourceOrTarget::Target, TEXT("LeftArm"), Wanted);
				Read = UWB_RetargetToolset::GetRetargetPoseBoneRotation(Retargeter, ERetargetSourceOrTarget::Target, TEXT("LeftArm"));
			});
			ExpectNoException();
			TestTrue(TEXT("Offset read back"), Read.Equals(Wanted, 0.01));

			Call([&]()
			{
				UWB_RetargetToolset::ResetRetargetPose(Retargeter, ERetargetSourceOrTarget::Target, {});
				Read = UWB_RetargetToolset::GetRetargetPoseBoneRotation(Retargeter, ERetargetSourceOrTarget::Target, TEXT("LeftArm"));
			});
			ExpectNoException();
			TestTrue(TEXT("Offset reset"), Read.IsNearlyZero(0.01));
		});

		It("auto-aligns the target pose", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			Call([&]() { UWB_RetargetToolset::AutoAlignRetargetPose(Retargeter, ERetargetSourceOrTarget::Target, ERetargetAutoAlignMethod::ChainToChain, {}); });
			ExpectNoException();
		});

		It("raises on a bone that is not in the skeleton", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::SetRetargetPoseBoneRotation(Retargeter, ERetargetSourceOrTarget::Target, TEXT("NoSuchBone"), FRotator::ZeroRotator); });
			ExpectExceptionContains(TEXT("is not in the skeleton"));
			Call([&]() { UWB_RetargetToolset::AutoAlignRetargetPose(Retargeter, ERetargetSourceOrTarget::Target, ERetargetAutoAlignMethod::ChainToChain, { TEXT("NoSuchBone") }); });
			ExpectExceptionContains(TEXT("is not in the skeleton"));
		});
	});

	Describe("RetargetAnimations", [this]()
	{
		It("creates an animation on the target skeleton", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			UAnimSequence* SourceAnim = LoadObject<UAnimSequence>(nullptr, SourceAnimPath);
			TestNotNull(TEXT("Source anim fixture"), SourceAnim);
			TArray<UObject*> Created;
			Call([&]()
			{
				Created = UWB_RetargetToolset::RetargetAnimations(Retargeter, { SourceAnim }, TestFolder, TEXT(""), TEXT(""), TEXT(""), TEXT("_Test"), false, true);
			});
			ExpectNoException();
			if (TestEqual(TEXT("Created count"), Created.Num(), 1))
			{
				const UAnimSequence* NewAnim = Cast<UAnimSequence>(Created[0]);
				if (TestNotNull(TEXT("Is an anim sequence"), NewAnim))
				{
					TestEqual(TEXT("Target skeleton"), NewAnim->GetSkeleton(), TargetMesh->GetSkeleton());
				}
			}
		});

		It("raises when no asset is given", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::RetargetAnimations(Retargeter, {}, TestFolder, TEXT(""), TEXT(""), TEXT(""), TEXT(""), false, true); });
			ExpectExceptionContains(TEXT("Assets must not be empty"));
		});

		It("raises on an asset that is not an animation", [this]()
		{
			UIKRetargeter* Retargeter = MakeRetargeter();
			ExpectNoException();
			Call([&]() { UWB_RetargetToolset::RetargetAnimations(Retargeter, { TargetMesh }, TestFolder, TEXT(""), TEXT(""), TEXT(""), TEXT(""), false, true); });
			ExpectExceptionContains(TEXT("is not an animation asset"));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
