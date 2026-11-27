#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Match/Roster/WB_TeamRosterTypes.h"
#include "WB_GymDirectorComponent.generated.h"

class UWB_GymDefinition;
class UWB_TeamRosterComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnGymLoaded, UWB_GymDefinition*, GymDefinition);

/**
 * Applies a gym (UWB_GymDefinition) to the match (devis gym-camera-p1, lot 2). Added to GM_Gym, a child of BP_SoccerGamemode.
 * At BeginPlay (before the GameMode Blueprint builds the roster) it loads the GymDefinition of the map's BP_GymSettings: roster composition
 * (UWB_TeamRosterComponent::SetSlotComposition), destruction switch. GM_Gym applies the timer and the camera from OnGymLoaded.
 * Public functions for the debug menu: LoadGymDefinition, SetSlotControl, SetSlotRole, GetActiveGymDefinition. The DataAsset is never changed:
 * the slot changes apply to a copy of its places.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_GymDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_GymDirectorComponent();

	/** Called after a gym was applied (BeginPlay, LoadGymDefinition, SetSlotControl). GM_Gym applies the timer here. */
	UPROPERTY(BlueprintAssignable, Category = "Gym")
	FWB_OnGymLoaded OnGymLoaded;

	/**
	 * Applies GymDefinition: composition, destruction switch, then OnGymLoaded. During the match, rebuilds the roster and puts everyone back
	 * to its place, without reloading the map. Returns false when GymDefinition is empty or the roster cannot be built (warning in the log).
	 */
	UFUNCTION(BlueprintCallable, Category = "Gym")
	bool LoadGymDefinition(UWB_GymDefinition* GymDefinition);

	/** Changes who drives the place SlotIndex (Gym.Control.Player or Gym.Control.Bot), then rebuilds the roster. Returns false when there is no such place. */
	UFUNCTION(BlueprintCallable, Category = "Gym")
	bool SetSlotControl(int32 SlotIndex, FGameplayTag ControlTag);

	/** Changes the forced role of the place SlotIndex (AI.ForcedRole.*, empty = normal roles), at once. Returns false when there is no such place. */
	UFUNCTION(BlueprintCallable, Category = "Gym")
	bool SetSlotRole(int32 SlotIndex, FGameplayTag RoleTag);

	/** The gym applied last. None before the first LoadGymDefinition. */
	UFUNCTION(BlueprintPure, Category = "Gym")
	UWB_GymDefinition* GetActiveGymDefinition() const { return ActiveGym; }

	/** Places of the active gym, with the changes of SetSlotControl and SetSlotRole. */
	UFUNCTION(BlueprintPure, Category = "Gym")
	TArray<FWB_RosterSlot> GetSlots() const { return Slots; }

	/** True when the active gym keeps the match timer running. True without an active gym (match behavior). */
	UFUNCTION(BlueprintPure, Category = "Gym")
	bool IsTimerEnabled() const;

	/** Camera mode of the active gym (Camera.Mode.*). Empty without an active gym, or when the gym leaves it to the GameMode. */
	UFUNCTION(BlueprintPure, Category = "Gym")
	FGameplayTag GetDefaultCameraMode() const;

	/** Gym director of the current GameMode. None outside GM_Gym. */
	UFUNCTION(BlueprintPure, Category = "Gym", meta = (WorldContext = "WorldContextObject"))
	static UWB_GymDirectorComponent* GetGymDirector(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWB_GymDefinition> ActiveGym;

	/** Copy of the places of ActiveGym, changed by SetSlotControl and SetSlotRole. */
	UPROPERTY(Transient)
	TArray<FWB_RosterSlot> Slots;

	/** Roster of the GameMode. Warning when missing. */
	UWB_TeamRosterComponent* GetRoster() const;

	/** Sends Slots to the roster and rebuilds it when it is already built. */
	bool ApplySlots();
};
