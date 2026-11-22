#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WB_DebugTextSubsystem.generated.h"

class APlayerController;
class UCanvas;

/**
 * Debug text in the world that stays on its actor when the camera constrains its aspect ratio (16:9 with black bars).
 * Use DrawDebugText instead of DrawDebugString: in UE 5.8 the engine places debug strings with the size of the whole window while the
 * image is the constrained rectangle, so they drift away from their actor (more the farther from the screen center).
 * Drawn through the "Game" debug draw service of the game and PIE viewports. Compiled out with the other debug draws (Shipping).
 */
UCLASS()
class WRECKINGBALL_API UWB_DebugTextSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Draws Text at Location, centered, for Duration seconds (0 or less: this frame only; call it every frame).
	 * BaseActor set: Location is an offset from the actor location, and the text follows the actor (removed when the actor is destroyed).
	 * Same use as Draw Debug String, but correct with a constrained aspect ratio. Development only: does nothing in Shipping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Debug", meta = (WorldContext = "WorldContextObject", DevelopmentOnly, AdvancedDisplay = "bDrawShadow,FontScale"))
	static void DrawDebugText(const UObject* WorldContextObject, FVector Location, const FString& Text, AActor* BaseActor = nullptr,
		FLinearColor Color = FLinearColor::White, float Duration = 0.f, bool bDrawShadow = true, float FontScale = 1.f);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FDebugTextEntry
	{
		FString Text;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<const AActor> BaseActor;
		bool bHasBaseActor = false;
		FColor Color = FColor::White;
		float FontScale = 1.f;
		bool bDrawShadow = true;
		/** World time after which the entry is removed (Duration > 0). */
		double ExpireTime = 0.0;
		/** Frame the entry was added on: a one-frame entry (Duration <= 0) only draws on that frame. */
		uint64 Frame = 0;
		bool bOneFrame = true;
	};

	TArray<FDebugTextEntry> Entries;
	FDelegateHandle DrawHandle;

	void Draw(UCanvas* Canvas, APlayerController* PlayerController);
};
