#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WB_TeamRosterComponent.generated.h"

class AWB_SoccerAIController_Base;
class AWB_TeamSpawnPoint;
class APlayerController;
class UWB_TeamRosterProfile;
class UWB_PlayerCommandComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWB_OnRosterCharacterAdded, APawn*, Character, int32, TeamIndex, int32, PlaceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnRosterCharacterRemoving, APawn*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWB_OnRosterCharacterSwitched, APlayerController*, PlayerController, APawn*, OldCharacter, APawn*, NewCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnRosterBotsToggled, bool, bBotsActive, int32, BotCount);

/**
 * The 3 versus 3 match roster (devis match 3v3, lot A). Added to BP_SoccerGamemode.
 * At kickoff, each team gets exactly RosterProfile->PlayersPerTeam characters: the humans (P1 = gamepad 1 + keyboard, P2..P4 = gamepads 2..4)
 * take the first places of their team in ID order, bots (BotControllerClass, each with its own AWB_SoccerPlayerState) fill the rest.
 * Each character keeps its place: it appears there at kickoff and after each goal (AWB_TeamSpawnPoint, else a circle around the origin).
 * A human switches to a teammate bot, or passes and takes the receiver, with SwitchOrPass. The bots stay still until ToggleBots
 * unless RosterProfile->bStartBotsOnPlay.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_TeamRosterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_TeamRosterComponent();

	/** Roster settings (DA_TeamRoster_Default). Required: empty, BuildRoster and SwitchOrPass are refused and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamRoster")
	TObjectPtr<UWB_TeamRosterProfile> RosterProfile;

	/** Controller class of the bot characters (BP_SoccerAIController, which holds the AI profile). Required to spawn the bots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamRoster")
	TSubclassOf<AWB_SoccerAIController_Base> BotControllerClass;

	/** Shows a short on-screen message when the bots are toggled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowMessages = true;

	/** Called for each character that joins the roster (BuildRoster, then each human added). Add it to the match lists here. */
	UPROPERTY(BlueprintAssignable, Category = "TeamRoster")
	FWB_OnRosterCharacterAdded OnRosterCharacterAdded;

	/** Called just before a character leaves the match and is destroyed (a human's spare pawn replaced in AddHumanCharacter). Remove it from the match lists here. */
	UPROPERTY(BlueprintAssignable, Category = "TeamRoster")
	FWB_OnRosterCharacterRemoving OnRosterCharacterRemoving;

	/** Called after a human switched character (SwitchOrPass, SwitchToCharacter). OldCharacter is now driven by a bot. */
	UPROPERTY(BlueprintAssignable, Category = "TeamRoster")
	FWB_OnRosterCharacterSwitched OnCharacterSwitched;

	/** Called after the bots were turned on or off. BotCount: number of characters driven by a bot. */
	UPROPERTY(BlueprintAssignable, Category = "TeamRoster")
	FWB_OnRosterBotsToggled OnBotsToggled;

	/** Creates a local player for each connected gamepad 2 to 4 that has none yet (P1 always exists: gamepad 1 + keyboard). Call before the teams are registered. Returns the number of players created. */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	int32 CreateGamepadPlayers();

	/**
	 * Builds the roster once the teams of the local players are registered (their AWB_SoccerPlayerState): humans first in ID order,
	 * then one bot character per free place, then everyone to its place. Bots start paused unless RosterProfile->bStartBotsOnPlay.
	 * Returns false when the roster cannot be built (warning in the log).
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool BuildRoster();

	/** True once BuildRoster succeeded. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster")
	bool IsRosterBuilt() const { return bBuilt; }

	/** Moves every character back to its place and stops it (kickoff after a goal). */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	void ResetCharactersToPlaces();

	/**
	 * Restart after the ball went out of bounds: moves every character to its kickoff place, the whole formation shifted so that
	 * the kickoff ball location (BP_KickoffPoint) lands on BallLocation, then stops it. Each place is put back on the NavMesh
	 * (else on the floor below), so a shifted place that falls in a wall or outside the arena still gets a reachable spot.
	 * Characters keep their kickoff facing.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	void ResetCharactersAroundBall(FVector BallLocation);

	/**
	 * The switch / pass button of PlayerController. Without the ball: takes the teammate bot pointed by the stick (nearest when the stick is at rest).
	 * With the ball: passes toward the stick (UWB_PlayerCommandComponent::RequestDirectionalPass: aim assist, or a free pass); when a bot drives the receiver, PlayerController takes it as soon as it catches the ball
	 * (the pass is cancelled if someone else catches it first). Pressed again while the pass travels: takes the receiver at once.
	 * Never an opponent, never another human's character. Refused during RosterProfile->SwitchCooldown after the last switch or pass.
	 * Returns true when a switch or a pass happened.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool SwitchOrPass(APlayerController* PlayerController);

	/**
	 * Release of the switch / pass button (charged pass, UWB_PassProfile::bUseChargedPass): sends the pass charged since SwitchOrPass,
	 * toward the stick at release, faster the longer the button was held. Does nothing when no pass is charging. Returns true when a pass left.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool ReleaseSwitchOrPass(APlayerController* PlayerController);

	/** Gives Target (a teammate driven by a bot) to PlayerController; the bot takes the character PlayerController leaves, which keeps its state. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool SwitchToCharacter(APlayerController* PlayerController, APawn* Target);

	/** Turns the bots on or off. Returns the new state. */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool ToggleBots();

	/** On: every bot character plays. Off: they stand still (their brain is paused). */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	void SetBotsActive(bool bActive);

	/** True while the bots play. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster")
	bool AreBotsActive() const { return bBotsActive; }

	/** Number of roster characters driven by a bot. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster")
	int32 GetBotCount() const;

	/**
	 * A local player joined during the match (its team is registered): it takes the bot character with the highest place of its team,
	 * and the pawn the GameMode gave it is destroyed (OnRosterCharacterRemoving first). Returns false when its team has no bot left.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool AddHumanCharacter(APlayerController* PlayerController);

	/** A local player leaves: a new bot takes its roster character, which stays in the match. Returns false when Character is not in the roster. */
	UFUNCTION(BlueprintCallable, Category = "TeamRoster")
	bool ReleaseHumanCharacter(APawn* Character);

	/** Every roster character, team 0 first. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster")
	TArray<APawn*> GetRosterCharacters() const;

	/** Team and place (0 = place 1) of a roster character. Returns false when Character is not in the roster. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster", meta = (ReturnDisplayName = "Found"))
	bool GetCharacterPlace(const APawn* Character, int32& TeamIndex, int32& PlaceIndex) const;

	/** Roster component of the current GameMode. None when the GameMode has none. */
	UFUNCTION(BlueprintPure, Category = "TeamRoster", meta = (WorldContext = "WorldContextObject"))
	static UWB_TeamRosterComponent* GetTeamRoster(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Passes the ball of Pawn toward Direction at Charge (RequestDirectionalPass), then records the switch time and the pending pass to a bot. */
	bool PassToward(APlayerController* PlayerController, APawn* Pawn, UWB_PlayerCommandComponent* Commands, const FVector& Direction, float Charge);

	/** A pass of a human toward a bot teammate, waiting for the receiver to catch the ball. */
	struct FPendingPass
	{
		TWeakObjectPtr<APlayerController> PlayerController;
		TWeakObjectPtr<APawn> Receiver;
		TWeakObjectPtr<AActor> Ball;
	};
	TArray<FPendingPass> PendingPasses;

	/** Per team: the human who held the ball last (chooses who takes a bot carrier when the team has several humans). */
	TMap<int32, TWeakObjectPtr<APlayerController>> LastHumanHolders;

	/** Human of TeamIndex who should take a bot carrier: the last human holder of the team, else the nearest to Near. None when the team has no human. */
	APlayerController* FindHumanForTeam(int32 TeamIndex, const APawn* Near) const;

	/**
	 * Called on each possession change. Ends a pending pass (the passer takes the receiver, or the pass is cancelled on an interception),
	 * then, with RosterProfile->bTakeBotCarrier, gives a bot teammate who takes the ball to a human of its team.
	 */
	UFUNCTION()
	void HandleBallPossessionChanged(AActor* Ball, AActor* NewPossessor, AActor* OldPossessor);

	struct FRosterEntry
	{
		TWeakObjectPtr<APawn> Character;
		int32 TeamIndex = -1;
		int32 PlaceIndex = 0;
	};
	TArray<FRosterEntry> Entries;

	bool bBuilt = false;
	bool bBotsActive = false;
	int32 SpawnedBotCount = 0;

	/** World time of the last switch or pass, per player. */
	TMap<TWeakObjectPtr<APlayerController>, double> LastSwitchTimes;

	mutable bool bWarnedNoProfile = false;

	/** True when RosterProfile is set (warning once otherwise). */
	bool HasProfile() const;

	/** Spawns a bot controller of TeamIndex with its own PlayerState, paused when the bots are off. */
	AWB_SoccerAIController_Base* SpawnBot(int32 TeamIndex, const FVector& Location, const FRotator& Rotation);

	/** Spawns a character driven by a new bot at place PlaceIndex of TeamIndex. */
	APawn* SpawnBotCharacter(int32 TeamIndex, int32 PlaceIndex, AWB_TeamSpawnPoint* SpawnPoint);

	/** Floor point and facing of a place: the spawn point place, else the fallback circle. */
	void ComputePlace(int32 TeamIndex, int32 PlaceIndex, const AWB_TeamSpawnPoint* SpawnPoint, FVector& OutFloor, FRotator& OutRotation) const;

	/** Fallback place: side by side on a circle around the origin, on the team's own half, projected on the NavMesh, facing the center. */
	void ComputeFallbackPlace(int32 TeamIndex, int32 PlaceIndex, FVector& OutFloor, FRotator& OutRotation) const;

	/** Teleports Character to a place and stops it. Offset (flat) shifts the place; a shifted place is put back on the NavMesh or the floor. */
	void PlaceCharacter(APawn* Character, int32 TeamIndex, int32 PlaceIndex, const AWB_TeamSpawnPoint* SpawnPoint, const FVector& Offset = FVector::ZeroVector) const;

	/** Moves Point onto the NavMesh (search box SideExtent wide), else onto the floor below it. Returns false when neither was found (Point unchanged). */
	bool ProjectToFloor(FVector& Point, float SideExtent) const;

	/** Moves every character to its place (shifted by Offset) and resets the bots' Dash cooldown. */
	void PlaceAllCharacters(const FVector& Offset);

	/** Spawn point of each team (warning when a team has none, or too few places). */
	void FindSpawnPoints(AWB_TeamSpawnPoint*& OutRed, AWB_TeamSpawnPoint*& OutBlue) const;

	const FRosterEntry* FindEntry(const APawn* Character) const;

	void ShowMessage(const FString& Message) const;
};
