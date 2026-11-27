// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Where a user tags an asset for WB Tools > Content > Collections: a "Collections" button in the toolbar of every asset editor
 * (Blueprint and Data Asset editors) and a "Collection Tags..." entry in the Content Browser asset menu. Both open the tag picker.
 */
class FWB_CollectionsUI
{
public:
	static void Register();
	static void Unregister();

	/** Window with the collection tags of the assets (several tags allowed) and the tags they inherit. Changes are written at once. */
	static void OpenTagPicker(const TArray<UObject*>& Assets);
};
