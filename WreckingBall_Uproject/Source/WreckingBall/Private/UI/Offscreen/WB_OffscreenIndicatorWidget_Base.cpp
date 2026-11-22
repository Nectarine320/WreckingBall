#include "UI/Offscreen/WB_OffscreenIndicatorWidget_Base.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Match/Roster/WB_TeamRosterComponent.h"
#include "Match/WB_TeamLibrary.h"
#include "UI/Offscreen/WB_OffscreenIndicatorItem_Base.h"
#include "UI/Offscreen/WB_OffscreenIndicatorProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBOffscreenIndicator, Log, All);

void UWB_OffscreenIndicatorWidget_Base::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!Profile || !Profile->ItemClass || !IndicatorCanvas)
	{
		UE_LOG(LogWBOffscreenIndicator, Warning, TEXT("%s: needs a Profile (DA_OffscreenIndicator_Default) with an ItemClass, and an IndicatorCanvas. No indicator."), *GetName());
		bWarnedNoProfile = true;
		return;
	}
	bIndicatorsEnabled = Profile->bShowIndicators;
	Items.Reset();
}

UWB_OffscreenIndicatorItem_Base* UWB_OffscreenIndicatorWidget_Base::AddItem()
{
	UWB_OffscreenIndicatorItem_Base* Item = CreateWidget<UWB_OffscreenIndicatorItem_Base>(this, Profile->ItemClass);
	if (!Item)
	{
		return nullptr;
	}
	if (UCanvasPanelSlot* CanvasSlot = IndicatorCanvas->AddChildToCanvas(Item))
	{
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetSize(FVector2D(Profile->SizeAtEdge));
	}
	Item->SetOutline(Profile->OutlineWidth, Profile->OutlineColor);
	Item->SetVisibility(ESlateVisibility::Collapsed);
	Items.Add(Item);
	return Item;
}

void UWB_OffscreenIndicatorWidget_Base::SetIndicatorsEnabled(bool bEnabled)
{
	bIndicatorsEnabled = bEnabled;
	if (!bEnabled)
	{
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			HideItem(i);
		}
	}
}

void UWB_OffscreenIndicatorWidget_Base::GatherCharacters(TArray<APawn*>& OutCharacters) const
{
	OutCharacters.Reset();
	if (const UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this); Roster && Roster->IsRosterBuilt())
	{
		OutCharacters = Roster->GetRosterCharacters();
	}
	else
	{
		// No 3v3 roster (other game modes): the local players' pawns.
		const UGameInstance* GameInstance = GetGameInstance();
		const int32 NumLocal = GameInstance ? GameInstance->GetNumLocalPlayers() : 0;
		for (int32 i = 0; i < NumLocal; ++i)
		{
			const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, i);
			if (APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr)
			{
				OutCharacters.Add(Pawn);
			}
		}
	}

	if (!Profile->bShowBots)
	{
		OutCharacters.RemoveAll([](const APawn* Pawn) { return !UWB_TeamLibrary::IsHumanControlled(Pawn); });
	}
}

void UWB_OffscreenIndicatorWidget_Base::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Profile || !Profile->ItemClass || !IndicatorCanvas || !bIndicatorsEnabled)
	{
		return;
	}

	TArray<APawn*> Characters;
	GatherCharacters(Characters);
	while (Items.Num() < Characters.Num() && AddItem())
	{
	}

	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	FVector2D AreaMin;
	FVector2D AreaMax;
	const bool bHasArea = GetPlayArea(ViewportScale, AreaMin, AreaMax);
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		UpdateItem(i, bHasArea && Characters.IsValidIndex(i) ? Characters[i] : nullptr, AreaMin, AreaMax, ViewportScale);
	}
}

bool UWB_OffscreenIndicatorWidget_Base::GetPlayArea(float ViewportScale, FVector2D& OutMin, FVector2D& OutMax) const
{
	// The rectangle the engine renders the view in (constrained to the camera aspect ratio, black bars excluded), in viewport pixels.
	const APlayerController* PlayerController = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData ProjectionData;
	if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport
		&& LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		const FIntRect Rect = ProjectionData.GetConstrainedViewRect();
		if (Rect.Width() > 0 && Rect.Height() > 0)
		{
			OutMin = FVector2D(Rect.Min) / ViewportScale;
			OutMax = FVector2D(Rect.Max) / ViewportScale;
			return true;
		}
	}

	// No projection data yet: the whole viewport.
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;
	OutMin = FVector2D::ZeroVector;
	OutMax = ViewportSize;
	return ViewportSize.X > 0.f && ViewportSize.Y > 0.f;
}

void UWB_OffscreenIndicatorWidget_Base::UpdateItem(int32 Index, const APawn* Pawn, const FVector2D& AreaMin, const FVector2D& AreaMax, float ViewportScale)
{
	APlayerController* ViewController = GetOwningPlayer();
	if (!Pawn || !ViewController)
	{
		HideItem(Index);
		return;
	}

	// Projected position, in the same space as the play area (whole viewport, UI units).
	FVector2D Screen;
	const bool bInFront = UGameplayStatics::ProjectWorldToScreen(ViewController, Pawn->GetActorLocation(), Screen, false);
	Screen /= ViewportScale;

	const float Threshold = Profile->OffscreenThreshold;
	const bool bOffscreen = !bInFront || Screen.X < AreaMin.X + Threshold || Screen.Y < AreaMin.Y + Threshold
		|| Screen.X > AreaMax.X - Threshold || Screen.Y > AreaMax.Y - Threshold;
	if (!bOffscreen)
	{
		HideItem(Index);
		return;
	}

	// Distance past the play area edge (as if the image went on): sets the size, from SizeAtEdge just outside to SizeFar at FarDistance.
	const float PastX = FMath::Max3(0.f, static_cast<float>(AreaMin.X - Screen.X), static_cast<float>(Screen.X - AreaMax.X));
	const float PastY = FMath::Max3(0.f, static_cast<float>(AreaMin.Y - Screen.Y), static_cast<float>(Screen.Y - AreaMax.Y));
	const float Alpha = bInFront ? FMath::Clamp(FMath::Sqrt(PastX * PastX + PastY * PastY) / FMath::Max(Profile->FarDistance, 1.f), 0.f, 1.f) : 1.f;
	const float Size = FMath::Lerp(Profile->SizeAtEdge, Profile->SizeFar, Alpha);

	// Rectangle the indicator center may use: the play area inset by each edge margin and half the indicator.
	const FMargin& Margins = Profile->EdgeMargins;
	const float HalfSize = Size * 0.5f;
	const FVector2D InnerMin(AreaMin.X + Margins.Left + HalfSize, AreaMin.Y + Margins.Top + HalfSize);
	FVector2D InnerMax(AreaMax.X - Margins.Right - HalfSize, AreaMax.Y - Margins.Bottom - HalfSize);
	// Margins larger than the area: collapse to its middle (division guard).
	InnerMax = FVector2D(FMath::Max(InnerMax.X, InnerMin.X), FMath::Max(InnerMax.Y, InnerMin.Y));

	// From the play area center toward the character, stopped on the inner rectangle. With unequal margins the rectangle is off center,
	// so the ray is clipped against each side separately.
	const FVector2D Center = (AreaMin + AreaMax) * 0.5f;
	FVector2D Direction = (Screen - Center) * (bInFront ? 1.f : -1.f);
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2D(0.f, 1.f);
	}
	const FVector2D Origin(FMath::Clamp(Center.X, InnerMin.X, InnerMax.X), FMath::Clamp(Center.Y, InnerMin.Y, InnerMax.Y));
	const float ScaleX = Direction.X > KINDA_SMALL_NUMBER ? (InnerMax.X - Origin.X) / Direction.X
		: Direction.X < -KINDA_SMALL_NUMBER ? (InnerMin.X - Origin.X) / Direction.X : TNumericLimits<float>::Max();
	const float ScaleY = Direction.Y > KINDA_SMALL_NUMBER ? (InnerMax.Y - Origin.Y) / Direction.Y
		: Direction.Y < -KINDA_SMALL_NUMBER ? (InnerMin.Y - Origin.Y) / Direction.Y : TNumericLimits<float>::Max();
	const FVector2D Position = Origin + Direction * FMath::Min(ScaleX, ScaleY);

	UWB_OffscreenIndicatorItem_Base* Item = Items[Index];
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Item->Slot))
	{
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(FVector2D(Size));
	}

	// Label: "P1".."P4" for a human (its local player number), none for a bot. Team color read every time: the team may change.
	FText LabelText = FText::GetEmpty();
	if (const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
	{
		LabelText = FText::FromString(FString::Printf(TEXT("P%d"), UGameplayStatics::GetPlayerControllerID(const_cast<APlayerController*>(PlayerController)) + 1));
	}
	FWB_TeamDefinition Definition;
	UWB_TeamLibrary::GetTeamDefinition(UWB_TeamLibrary::GetTeamOf(Pawn), Definition);
	Item->SetIndicator(LabelText, Definition.IndicatorColor);
	Item->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWB_OffscreenIndicatorWidget_Base::HideItem(int32 Index)
{
	if (Items.IsValidIndex(Index) && Items[Index] && Items[Index]->GetVisibility() != ESlateVisibility::Collapsed)
	{
		Items[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
