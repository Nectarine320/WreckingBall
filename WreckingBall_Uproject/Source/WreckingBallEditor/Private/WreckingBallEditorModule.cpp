// Fill out your copyright notice in the Description page of Project Settings.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Retarget/WB_RetargetToolset.h"

/** Editor module of Wrecking Ball: registers the project's MCP toolsets. */
class FWreckingBallEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UWB_RetargetToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UWB_RetargetToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FWreckingBallEditorModule, WreckingBallEditor);
