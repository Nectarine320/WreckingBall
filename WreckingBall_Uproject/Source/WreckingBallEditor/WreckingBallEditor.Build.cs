// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

// Editor-only module: tools for the editor and the unreal-mcp agent. Never packaged in the game.
public class WreckingBallEditor : ModuleRules
{
	public WreckingBallEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// MCP toolsets: UToolsetDefinition and its registry.
		PrivateDependencyModuleNames.Add("ToolsetRegistry");

		// Retarget toolset: IK Rig, IK Retargeter and batch retarget. IKRig and IKRigEditor are public
		// because the toolset header uses their enums (ERetargetSourceOrTarget, ERetargetAutoAlignMethod).
		PublicDependencyModuleNames.AddRange(new string[] { "IKRig", "IKRigEditor" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetTools", "AssetRegistry", "EditorScriptingUtilities" });

		// IKRigEditor headers (retargeter controller) pull Slate widget classes.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
