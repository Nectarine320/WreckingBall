#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_ScreenWidgetLibrary.generated.h"

class UWidgetComponent;

/**
 * Helpers for screen-space widget components on characters (overheads, labels).
 * The match uses one shared camera: only the first local player's screen is shown. A screen-space widget component is drawn
 * in the screen of the local player that controls its pawn, so the widget of a character driven by P2-P4 would never be seen.
 */
UCLASS()
class WRECKINGBALL_API UWB_ScreenWidgetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Draws WidgetComponent in the first local player's screen (the one shown by the shared match camera), whoever controls its pawn.
	 * Call it once, at BeginPlay. Does nothing for a world-space widget component. Returns false (with a log warning) when there is no local player yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Screen Widget")
	static bool PinWidgetToSharedScreen(UWidgetComponent* WidgetComponent);
};
