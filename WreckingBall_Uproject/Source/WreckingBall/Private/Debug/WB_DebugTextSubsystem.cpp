#include "Debug/WB_DebugTextSubsystem.h"

#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

void UWB_DebugTextSubsystem::DrawDebugText(const UObject* WorldContextObject, FVector Location, const FString& Text, AActor* BaseActor,
	FLinearColor Color, float Duration, bool bDrawShadow, float FontScale)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	UWB_DebugTextSubsystem* Subsystem = World->GetSubsystem<UWB_DebugTextSubsystem>();
	if (!Subsystem)
	{
		// Editor worlds (no game viewport): the engine debug string is fine there.
		DrawDebugString(World, Location, Text, BaseActor, Color.ToFColor(true), Duration, bDrawShadow, FontScale);
		return;
	}

	FDebugTextEntry& Entry = Subsystem->Entries.AddDefaulted_GetRef();
	Entry.Text = Text;
	Entry.Location = Location;
	Entry.BaseActor = BaseActor;
	Entry.bHasBaseActor = BaseActor != nullptr;
	Entry.Color = Color.ToFColor(true);
	Entry.FontScale = FontScale;
	Entry.bDrawShadow = bDrawShadow;
	Entry.bOneFrame = Duration <= 0.f;
	Entry.ExpireTime = World->GetTimeSeconds() + Duration;
	Entry.Frame = GFrameCounter;
#endif
}

bool UWB_DebugTextSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWB_DebugTextSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if ENABLE_DRAW_DEBUG
	DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UWB_DebugTextSubsystem::Draw));
#endif
}

void UWB_DebugTextSubsystem::Deinitialize()
{
#if ENABLE_DRAW_DEBUG
	UDebugDrawService::Unregister(DrawHandle);
	DrawHandle.Reset();
#endif
	Entries.Reset();
	Super::Deinitialize();
}

void UWB_DebugTextSubsystem::Draw(UCanvas* Canvas, APlayerController* PlayerController)
{
#if ENABLE_DRAW_DEBUG
	// The service calls every registered world for every viewport: only draw in our own world's viewport.
	const UWorld* World = GetWorld();
	if (!Canvas || !Canvas->SceneView || !PlayerController || !World || PlayerController->GetWorld() != World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	Entries.RemoveAll([Now](const FDebugTextEntry& Entry)
	{
		const bool bExpired = Entry.bOneFrame ? Entry.Frame != GFrameCounter : Now > Entry.ExpireTime;
		return bExpired || (Entry.bHasBaseActor && !Entry.BaseActor.IsValid());
	});

	// The debug canvas is moved to the image origin but keeps the window size (ClipX, ClipY), so UCanvas::Project is wrong when the
	// camera constrains its aspect ratio. WorldToPixel uses the image rectangle; minus its origin, it is in canvas space (pixels).
	const FSceneView* View = Canvas->SceneView;
	const FVector2D ViewOrigin(View->UnscaledViewRect.Min);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	for (const FDebugTextEntry& Entry : Entries)
	{
		const AActor* Base = Entry.BaseActor.Get();
		const FVector WorldLocation = Base ? Base->GetActorLocation() + Entry.Location : Entry.Location;
		FVector2D Pixel;
		if (View->WorldToScreen(WorldLocation).W <= 0.f || !View->WorldToPixel(WorldLocation, Pixel))
		{
			continue;
		}
		const FVector2D Position = Pixel - ViewOrigin;

		FCanvasTextItem Item(Position, FText::FromString(Entry.Text), Font, FLinearColor(Entry.Color));
		Item.Scale = FVector2D(Entry.FontScale);
		Item.bCentreX = true;
		Item.bCentreY = true;
		if (Entry.bDrawShadow)
		{
			Item.EnableShadow(FLinearColor::Black);
		}
		Canvas->DrawItem(Item);
	}
#endif
}
