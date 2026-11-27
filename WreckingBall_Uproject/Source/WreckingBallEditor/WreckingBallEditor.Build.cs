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

		// IKRigEditor headers (retargeter controller) pull Slate widget classes. Also used by the WB Tools panels.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// WB Tools (devis wb-tools): the game module (gym classes, team spawn point), the editor main menu,
		// the project settings, Editor Utility Widgets, Blueprint graph nodes, Enhanced Input and the clipboard.
		PublicDependencyModuleNames.AddRange(new string[] { "WreckingBall", "DeveloperSettings", "GameplayTags" });
		PrivateDependencyModuleNames.AddRange(new string[] { "ToolMenus", "Blutility", "UMG", "UMGEditor", "BlueprintGraph", "EnhancedInput", "InputCore", "ApplicationCore" });

		// WB Tools > Content > Collections: local collections, the Gameplay Tag picker, the Content Browser asset menu.
		PrivateDependencyModuleNames.AddRange(new string[] { "CollectionManager", "GameplayTagsEditor", "ContentBrowser" });
	}
}
