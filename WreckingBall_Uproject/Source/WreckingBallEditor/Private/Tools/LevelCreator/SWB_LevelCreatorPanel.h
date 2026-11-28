// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/LevelCreator/WB_LevelCreatorLibrary.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

/** WB Tools > Level > Create Level: a 3-character code and a name, a live check, and Create. */
class SWB_LevelCreatorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWB_LevelCreatorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Recheck();
	FReply OnCreate();

	FString Code;
	FString Name;
	FWB_NewLevelCheck Check;
	TSharedPtr<STextBlock> ResultText;
};
