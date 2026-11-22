#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WB_OffscreenIndicatorWidget_Base.generated.h"

class UCanvasPanel;
class UWB_OffscreenIndicatorItem_Base;
class UWB_OffscreenIndicatorProfile;

/**
 * Off-screen indicators of the match characters (devis match 3v3, lot B). Child: WBP_OffscreenIndicators, a full-screen widget whose root
 * holds a CanvasPanel named IndicatorCanvas; added to the viewport once, by the PlayerController of P1 (shared screen).
 * Each frame, every match character (the roster of UWB_TeamRosterComponent, else the local players' pawns) is projected on screen: when it is
 * out of the frame, an indicator (Profile->ItemClass, pool grown as needed) sits on the screen edge in its direction, with the team IndicatorColor,
 * "P1".."P4" on a human and no text on a bot (Profile->bShowBots). Its size goes from SizeAtEdge to SizeFar with the distance past the edge.
 */
UCLASS(Abstract, Blueprintable)
class WRECKINGBALL_API UWB_OffscreenIndicatorWidget_Base : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Indicator settings (DA_OffscreenIndicator_Default). Required: empty, nothing shows and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
	TObjectPtr<UWB_OffscreenIndicatorProfile> Profile;

	/** Turns the indicators on or off (starts with Profile->bShowIndicators). */
	UFUNCTION(BlueprintCallable, Category = "Indicator")
	void SetIndicatorsEnabled(bool bEnabled);

	/** True while the indicators are on. */
	UFUNCTION(BlueprintPure, Category = "Indicator")
	bool AreIndicatorsEnabled() const { return bIndicatorsEnabled; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Full-screen canvas that receives the indicators. */
	UPROPERTY(BlueprintReadOnly, Category = "Indicator", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> IndicatorCanvas;

private:
	/** Indicator pool: Items[i] follows the i-th character of the frame. Grown when there are more characters. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWB_OffscreenIndicatorItem_Base>> Items;

	/** Adds one hidden indicator to the pool. Returns it, or null when it cannot be created. */
	UWB_OffscreenIndicatorItem_Base* AddItem();

	/** Characters that may get an indicator this frame. */
	void GatherCharacters(TArray<APawn*>& OutCharacters) const;

	bool bIndicatorsEnabled = true;
	bool bWarnedNoProfile = false;

	/**
	 * Play area of the owning player, in UI units (canvas space): the image the camera draws, without the black bars when it constrains its
	 * aspect ratio. False when there is no viewport yet.
	 */
	bool GetPlayArea(float ViewportScale, FVector2D& OutMin, FVector2D& OutMax) const;

	/** Places Items[Index] at the play area edge toward Pawn, or hides it when Pawn is inside the play area. */
	void UpdateItem(int32 Index, const APawn* Pawn, const FVector2D& AreaMin, const FVector2D& AreaMax, float ViewportScale);

	void HideItem(int32 Index);
};
