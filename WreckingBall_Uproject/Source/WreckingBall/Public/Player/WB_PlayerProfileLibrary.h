#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_PlayerProfileLibrary.generated.h"

class UWB_PlayerDashProfile;
class UWB_PlayerEnergyProfile;

/**
 * One-node access to the tuning profiles of a player (lot P of Phase C). In a player Blueprint the Player pin defaults to self,
 * so a setting reads as two nodes: Get Player Energy Profile -> Get SprintDuration.
 */
UCLASS()
class WRECKINGBALL_API UWB_PlayerProfileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Energy profile of Player (EnergyProfile of its UWB_PlayerStateComponent). Null, with a warning in the log, when the component or the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerProfile", meta = (DefaultToSelf = "Player"))
	static const UWB_PlayerEnergyProfile* GetPlayerEnergyProfile(const AActor* Player);

	/** Dash profile of Player (DashProfile of its UWB_PlayerCommandComponent). Null, with a warning in the log, when the component or the profile is missing. */
	UFUNCTION(BlueprintPure, Category = "PlayerProfile", meta = (DefaultToSelf = "Player"))
	static const UWB_PlayerDashProfile* GetPlayerDashProfile(const AActor* Player);
};
