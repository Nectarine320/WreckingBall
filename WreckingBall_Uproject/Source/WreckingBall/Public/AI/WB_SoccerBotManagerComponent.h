#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_SoccerBotManagerComponent.generated.h"

class AWB_SoccerAIController_Base;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnLocalPlayerAdded, int32, PlayerIndex, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnLocalPlayerRemoving, int32, PlayerIndex, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnBotsToggled, bool, bBotsActive, int32, BotCount);

/**
 * Adds and removes local players, and hands the players without a gamepad to bots (AWB_SoccerAIController_Base).
 * Added to BP_SoccerGamemode. Player 0 (keyboard) always stays human. A bot possesses the pawn in place of its
 * PlayerController, which takes the pawn back when the bots are turned off.
 * The GameMode Blueprint registers a new player (team, material) on OnLocalPlayerAdded, and cleans it up on OnLocalPlayerRemoving.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_SoccerBotManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_SoccerBotManagerComponent();

	/** Most local players AddLocalPlayer allows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPlayers = 4;

	/** RemoveLastLocalPlayer never goes below this count (players 1 and 2 stay). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MinPlayers = 2;

	/** Controller class spawned for each bot (a Blueprint child can set the AI profile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots")
	TSubclassOf<AWB_SoccerAIController_Base> BotControllerClass;

	/** A PlayerStart counts as free when no pawn stands within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots", meta = (ClampMin = "0"))
	float PlayerStartClearance = 150.f;

	/** Without a free PlayerStart, a new player appears this far beside player 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots", meta = (ClampMin = "0"))
	float SpawnBesideDistance = 200.f;

	/** Shows a short on-screen message on each add, remove and toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoccerBots")
	bool bShowMessages = true;

	/** Called after a local player and its pawn were created and placed, before a bot takes it. Register the player here (team, material). */
	UPROPERTY(BlueprintAssignable, Category = "SoccerBots")
	FWB_OnLocalPlayerAdded OnLocalPlayerAdded;

	/** Called just before a local player and its pawn are destroyed. Remove the pawn from the match lists here. */
	UPROPERTY(BlueprintAssignable, Category = "SoccerBots")
	FWB_OnLocalPlayerRemoving OnLocalPlayerRemoving;

	/** Called after the bots were turned on or off. BotCount: number of pawns now driven by a bot. */
	UPROPERTY(BlueprintAssignable, Category = "SoccerBots")
	FWB_OnBotsToggled OnBotsToggled;

	/** Creates a local player (PlayerController and pawn) on a free PlayerStart, else beside player 1. While bots are on, a bot takes it at once when it has no gamepad. Returns its player index, -1 when refused. */
	UFUNCTION(BlueprintCallable, Category = "SoccerBots")
	int32 AddLocalPlayer();

	/** Removes the last local player and its pawn, never below MinPlayers. Returns true when a player was removed. */
	UFUNCTION(BlueprintCallable, Category = "SoccerBots")
	bool RemoveLastLocalPlayer();

	/** Turns the bots on or off. Returns the new state. */
	UFUNCTION(BlueprintCallable, Category = "SoccerBots")
	bool ToggleBots();

	/** On: a bot takes every local player except player 0 that has no connected gamepad. Off: every pawn goes back to its PlayerController. */
	UFUNCTION(BlueprintCallable, Category = "SoccerBots")
	void SetBotsActive(bool bActive);

	/** True while the bots are on. */
	UFUNCTION(BlueprintPure, Category = "SoccerBots")
	bool AreBotsActive() const { return bBotsActive; }

	/** Number of pawns currently driven by a bot. */
	UFUNCTION(BlueprintPure, Category = "SoccerBots")
	int32 GetBotCount() const;

	/** Number of local players. */
	UFUNCTION(BlueprintPure, Category = "SoccerBots")
	int32 GetNumLocalPlayers() const;

	/** True when a connected input device (gamepad) is assigned to the platform user of PlayerController. */
	UFUNCTION(BlueprintPure, Category = "SoccerBots")
	static bool HasGamepadAssigned(const APlayerController* PlayerController);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** A PlayerController whose pawn is driven by a bot. */
	struct FBotLink
	{
		TWeakObjectPtr<APlayerController> PlayerController;
		TWeakObjectPtr<AWB_SoccerAIController_Base> Bot;
	};
	TArray<FBotLink> BotLinks;

	bool bBotsActive = false;

	/** Gives the pawn of PlayerController to a new bot. */
	bool PossessWithBot(APlayerController* PlayerController);

	/** Destroys the bot of BotLinks[Index]; gives the pawn back to the PlayerController when bReturnPawn. */
	void ReleaseBot(int32 Index, bool bReturnPawn);

	/** Index in BotLinks of the bot of PlayerController, -1 when none. */
	int32 FindBotLink(const APlayerController* PlayerController) const;

	/** Moves a new pawn to a free PlayerStart, else beside player 1. */
	void PlaceNewPawn(APawn* Pawn, int32 PlayerIndex) const;

	void ShowMessage(const FString& Message) const;
};
