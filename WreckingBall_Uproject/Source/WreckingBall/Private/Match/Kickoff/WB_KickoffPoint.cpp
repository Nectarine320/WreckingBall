#include "Match/Kickoff/WB_KickoffPoint.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBKickoff, Log, All);

AWB_KickoffPoint::AWB_KickoffPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Editor preview only (same layout as the team spawn point labels).
	PreviewLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PreviewLabel"));
	PreviewLabel->SetupAttachment(Root);
	PreviewLabel->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	PreviewLabel->SetRelativeRotation(FRotator(90.f, 180.f, 0.f));
	PreviewLabel->SetHorizontalAlignment(EHTA_Center);
	PreviewLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PreviewLabel->SetWorldSize(60.f);
	PreviewLabel->SetText(FText::FromString(TEXT("KICKOFF")));
	PreviewLabel->SetHiddenInGame(true);
	PreviewLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AWB_KickoffPoint* AWB_KickoffPoint::FindKickoffPoint(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AWB_KickoffPoint* Found = nullptr;
	for (TActorIterator<AWB_KickoffPoint> It(World); It; ++It)
	{
		if (!Found)
		{
			Found = *It;
		}
		else
		{
			UE_LOG(LogWBKickoff, Warning, TEXT("Several kickoff points (%s, %s): %s is used."), *Found->GetName(), *It->GetName(), *Found->GetName());
		}
	}
	return Found;
}
