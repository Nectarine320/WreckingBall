// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/LevelCreator/SWB_LevelCreatorPanel.h"

#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Tools/WB_ToolsStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WBLevelCreatorPanel"

void SWB_LevelCreatorPanel::Construct(const FArguments& InArgs)
{
	auto Label = [](const FText& Text)
	{
		return SNew(SBox).WidthOverride(90.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text)];
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("Help", "Creates a level from the template (Lv_Template): the persistent level and its Layout, Gameplay and Light sub-levels, ready for a match. 3 digits = official level, 3 letters = gym (gym mode on)."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Code", "Code"))]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(80.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("CodeHint", "200 or ABC"))
					.ToolTipText(LOCTEXT("CodeTip", "Exactly 3 digits (official level) or 3 letters (gym). Not both."))
					.Text_Lambda([this]() { return FText::FromString(Code); })
					.OnVerifyTextChanged_Lambda([](const FText& Text, FText& OutError)
					{
						// Never more than 3 characters in the box.
						if (Text.ToString().Len() > 3)
						{
							OutError = LOCTEXT("TooLong", "3 characters at most");
							return false;
						}
						return true;
					})
					.OnTextChanged_Lambda([this](const FText& Text) { Code = Text.ToString().Left(3); Recheck(); })
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Name", "Name"))]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("NameHint", "e.g. Bungalow or SoccerField"))
				.ToolTipText(LOCTEXT("NameTip", "Letters and digits, starting with a letter, no spaces."))
				.Text_Lambda([this]() { return FText::FromString(Name); })
				.OnTextChanged_Lambda([this](const FText& Text) { Name = Text.ToString(); Recheck(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity_Lambda([this]() { return Check.bValid ? FSlateColor(WBToolsStyle::Highlight) : FSlateColor(WBToolsStyle::Error); })
			.Text_Lambda([this]()
			{
				if (Code.IsEmpty() && Name.IsEmpty())
				{
					return FText::GetEmpty();
				}
				if (!Check.bValid)
				{
					return FText::FromString(Check.Error);
				}
				TArray<FString> Lines;
				Lines.Add(FString::Printf(TEXT("Will create %s%s"), *Check.MapPackage, Check.bGym ? TEXT("  (gym, gym mode on)") : TEXT("")));
				for (const FString& Sub : Check.SubLevelPackages)
				{
					Lines.Add(TEXT("  + ") + FPackageName::GetShortName(Sub));
				}
				return FText::FromString(FString::Join(Lines, TEXT("\n")));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("Create", "Create Level"))
			.IsEnabled_Lambda([this]() { return Check.bValid; })
			.OnClicked(this, &SWB_LevelCreatorPanel::OnCreate)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SAssignNew(ResultText, STextBlock).AutoWrapText(true)
		]
	];
}

void SWB_LevelCreatorPanel::Recheck()
{
	Check = UWB_LevelCreatorLibrary::CheckNewLevel(Code, Name);
}

FReply SWB_LevelCreatorPanel::OnCreate()
{
	ResultText->SetText(FText::FromString(UWB_LevelCreatorLibrary::CreateLevel(Code, Name, /*bPromptSave=*/true)));
	Recheck();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
