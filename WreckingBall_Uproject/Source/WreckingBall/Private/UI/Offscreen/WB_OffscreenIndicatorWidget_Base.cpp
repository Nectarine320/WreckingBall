#include "UI/Offscreen/WB_OffscreenIndicatorWidget_Base.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
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
	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		UpdateItem(i, Characters.IsValidIndex(i) ? Characters[i] : nullptr, ScreenSize, ViewportScale);
	}
}

void UWB_OffscreenIndicatorWidget_Base::UpdateItem(int32 Index, const APawn* Pawn, const FVector2D& ScreenSize, float ViewportScale)
{
	APlayerController* ViewController = GetOwningPlayer();
	if (!Pawn || !ViewController || ScreenSize.X <= 0.f || ScreenSize.Y <= 0.f)
	{
		HideItem(Index);
		return;
	}

	FVector2D Screen;
	const bool bInFront = UGameplayStatics::ProjectWorldToScreen(ViewController, Pawn->GetActorLocation(), Screen, false);
	Screen /= ViewportScale;

	const float Threshold = Profile->OffscreenThreshold;
	const bool bOffscreen = !bInFront || Screen.X < Threshold || Screen.Y < Threshold || Screen.X > ScreenSize.X - Threshold || Screen.Y > ScreenSize.Y - Threshold;
	if (!bOffscreen)
	{
		HideItem(Index);
		return;
	}

	// Distance past the screen edge (as if the screen went on): sets the size, from SizeAtEdge just outside to SizeFar at FarDistance.
	const float PastX = FMath::Max3(0.f, static_cast<float>(-Screen.X), static_cast<float>(Screen.X - ScreenSize.X));
	const float PastY = FMath::Max3(0.f, static_cast<float>(-Screen.Y), static_cast<float>(Screen.Y - ScreenSize.Y));
	const float Alpha = bInFront ? FMath::Clamp(FMath::Sqrt(PastX * PastX + PastY * PastY) / FMath::Max(Profile->FarDistance, 1.f), 0.f, 1.f) : 1.f;
	const float Size = FMath::Lerp(Profile->SizeAtEdge, Profile->SizeFar, Alpha);

	// From the screen center toward the character, stopped on a rectangle inset by the margin and half the indicator.
	const FVector2D Center = ScreenSize * 0.5f;
	FVector2D Direction = (Screen - Center) * (bInFront ? 1.f : -1.f);
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2D(0.f, 1.f);
	}
	const FVector2D Half = Center - FVector2D(Profile->EdgeMargin + Size * 0.5f);
	const float ScaleX = FMath::Abs(Direction.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Direction.X) : TNumericLimits<float>::Max();
	const float ScaleY = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Direction.Y) : TNumericLimits<float>::Max();
	const FVector2D Position = Center + Direction * FMath::Min(ScaleX, ScaleY);

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
