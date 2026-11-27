// Fill out your copyright notice in the Description page of Project Settings.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Retarget/WB_RetargetToolset.h"
#include "Tools/Collections/WB_CollectionsLibrary.h"
#include "Tools/Collections/WB_CollectionsUI.h"
#include "Tools/Gym/WB_GymSelectorLibrary.h"
#include "Tools/Menu/WB_ToolsMenu.h"
#include "Tools/WB_ToolsToolset.h"

/** Editor module of Wrecking Ball: registers the project's MCP toolsets and the WB Tools editor menu. */
class FWreckingBallEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UWB_RetargetToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UWB_ToolsToolset::StaticClass());
		FWB_ToolsMenu::Register();
		UWB_GymSelectorLibrary::ApplyPlayer1CameraPreference();
		UWB_CollectionsLibrary::RegisterAssetRegistryTags();
		FWB_CollectionsUI::Register();
	}

	virtual void ShutdownModule() override
	{
		FWB_CollectionsUI::Unregister();
		UWB_CollectionsLibrary::UnregisterAssetRegistryTags();
		FWB_ToolsMenu::Unregister();
		UToolsetRegistry::UnregisterToolsetClass(UWB_ToolsToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UWB_RetargetToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FWreckingBallEditorModule, WreckingBallEditor);
