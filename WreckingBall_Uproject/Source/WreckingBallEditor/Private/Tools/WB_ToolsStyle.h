// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

/** Display colors shared by the WB Tools panels (editor UI only, no gameplay effect). */
namespace WBToolsStyle
{
	/** Something to check (no description, unsorted actor, reference that breaks). */
	inline const FLinearColor Warning(1.0f, 0.55f, 0.1f);

	/** Something that must not stay like this (map committed in gym mode, key used twice). */
	inline const FLinearColor Error(1.0f, 0.3f, 0.3f);

	/** The current item (the map open in the editor). */
	inline const FLinearColor Highlight(1.0f, 0.82f, 0.2f);

	/** Secondary text (paths, counts). */
	inline const FLinearColor Dim(0.6f, 0.6f, 0.6f);
}
