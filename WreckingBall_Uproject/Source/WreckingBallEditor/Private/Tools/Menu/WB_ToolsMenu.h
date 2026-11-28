// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * WB Tools menu of the editor main menu bar, next to Help (devis wb-tools), and the tabs of its built-in tools.
 * Sections Level, Animation, Gym, Debug; extra entries come from DA_WBToolsMenu (Project Settings > Editor > WB Tools).
 */
class FWB_ToolsMenu
{
public:
	static void Register();
	static void Unregister();

	/** Opens (or brings to front) the tab of a built-in tool. */
	static void OpenTab(FName TabId);

	static const FName SortLevelTabId;
	static const FName GymTabId;
	static const FName DebugKeysTabId;
	static const FName CollectionsTabId;
	static const FName RetargetTabId;
	static const FName LevelCreatorTabId;
};
