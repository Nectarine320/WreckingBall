#include "UI/WB_ScreenWidgetLibrary.h"

#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBScreenWidget, Log, All);

bool UWB_ScreenWidgetLibrary::PinWidgetToSharedScreen(UWidgetComponent* WidgetComponent)
{
	if (!WidgetComponent || WidgetComponent->GetWidgetSpace() != EWidgetSpace::Screen)
	{
		return false;
	}
	const UWorld* World = WidgetComponent->GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	ULocalPlayer* FirstPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	if (!FirstPlayer)
	{
		UE_LOG(LogWBScreenWidget, Warning, TEXT("PinWidgetToSharedScreen: no local player for %s, the widget keeps its default screen."), *GetPathNameSafe(WidgetComponent));
		return false;
	}
	// The widget moves to this player's screen layer on its next update, even when it was already added to another one.
	WidgetComponent->SetOwnerPlayer(FirstPlayer);
	return true;
}
