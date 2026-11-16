#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WB_PlayerStateComponent.generated.h"

class UWB_PlayerStateProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnPlayerStateChanged, FGameplayTag, OldState, FGameplayTag, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWB_OnHoldingChanged, FGameplayTag, OldHolding, FGameplayTag, NewHolding);

/**
 * Single source of truth of what a player does (Player.State.*) and holds (Player.Holding.*), Phase C step 9.
 * Added to BP_GenericCharacter. The two values are read-only from Blueprint: they change only through
 * ChangePlayerState (checked against the profile's transition table) and ChangeHolding, which fire the dispatchers.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_PlayerStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_PlayerStateComponent();

	/** What the player is doing (Player.State.*). Change it only with ChangePlayerState. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PlayerState", meta = (Categories = "Player.State"))
	FGameplayTag CurrentState;

	/** What the player holds (Player.Holding.*). Change it only with ChangeHolding. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PlayerState", meta = (Categories = "Player.Holding"))
	FGameplayTag CurrentHolding;

	/** State rules and durations (DA_PlayerState_Default). Empty: the class defaults of UWB_PlayerStateProfile are used (same table as the quote). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerState")
	TObjectPtr<UWB_PlayerStateProfile> PlayerStateProfile;

	/** Called after CurrentState changed. Bind it instead of reading CurrentState every frame (HUD, AnimBP). */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState")
	FWB_OnPlayerStateChanged OnPlayerStateChanged;

	/** Called after CurrentHolding changed. */
	UPROPERTY(BlueprintAssignable, Category = "PlayerState")
	FWB_OnHoldingChanged OnHoldingChanged;

	/** Shows the current state and holding above the player (development builds only). Off by default. Use SetDebugShowState to toggle it during play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebugShowState = false;

	/** Height in cm above the actor location where the debug text is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (Units = "Centimeters"))
	float DebugTextHeight = 120.f;

	/**
	 * Tries to enter NewState. Refused (returns false, nothing changes) when CanEnterState is false.
	 * On success, fires OnPlayerStateChanged(OldState, NewState). The only way to change CurrentState.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Changed"))
	bool ChangePlayerState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState);

	/**
	 * Sets what the player holds. Returns false when NewHolding is not a Player.Holding.* tag or is already held.
	 * On success, fires OnHoldingChanged(OldHolding, NewHolding). The only way to change CurrentHolding.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayerState", meta = (ReturnDisplayName = "Changed"))
	bool ChangeHolding(UPARAM(meta = (Categories = "Player.Holding")) FGameplayTag NewHolding);

	/** True when ChangePlayerState(NewState) would succeed: allowed by the transition table from CurrentState, and the hand rules (empty hands / holding) are met. */
	UFUNCTION(BlueprintPure, Category = "PlayerState", meta = (ReturnDisplayName = "Allowed"))
	bool CanEnterState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag NewState) const;

	/** True when CurrentState is State (or a child of it). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool IsInState(UPARAM(meta = (Categories = "Player.State")) FGameplayTag State) const;

	/** True when the player holds something (CurrentHolding is not Player.Holding.None). */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	bool IsHoldingAnything() const;

	/** The profile in use: PlayerStateProfile, or the UWB_PlayerStateProfile class defaults when it is empty. Never null. */
	UFUNCTION(BlueprintPure, Category = "PlayerState")
	const UWB_PlayerStateProfile* GetActiveProfile() const;

	/** Turns the debug text above the player on or off during play. */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetDebugShowState(bool bShow);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
};
