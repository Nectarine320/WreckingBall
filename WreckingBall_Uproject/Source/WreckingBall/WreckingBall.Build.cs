// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class WreckingBall : ModuleRules
{
	public WreckingBall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		// Lot 9: per-piece Geometry Collection debris (component, removal facades, physics proxy).
		PrivateDependencyModuleNames.AddRange(new string[] { "GeometryCollectionEngine", "Chaos" });

		// Lot 10: physical materials and surface types for impact feedback.
		PrivateDependencyModuleNames.Add("PhysicsCore");

		// Lot 10: player feedback catalog (Gameplay Tags, project setting).
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTags", "DeveloperSettings" });

		// Footstep dust puff anim notify (UWB_AnimNotify_FootstepPuff spawns a Niagara system).
		PrivateDependencyModuleNames.Add("Niagara");

		// Energy bar widget base class (UWB_EnergyBarWidget_Base derives from UUserWidget).
		PublicDependencyModuleNames.Add("UMG");
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Soccer bots: AWB_SoccerAIController_Base derives from AAIController (public header), NavMesh paths,
		// and the input device mapper to tell which local players have a gamepad.
		PublicDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks" });
		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem", "ApplicationCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
