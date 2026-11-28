#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_DataReadLibrary.generated.h"

/**
 * Reads variables of Blueprint data assets by name (PDA_PhysicsProfile of the balls, PDA_DestructibleProfile of the walls), for the C++ profiles
 * and their editor previews. Blueprint data asset variables have no C++ type, so they are found by reflection.
 */
UCLASS()
class WRECKINGBALL_API UWB_DataReadLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the number (float, double or integer) variable Variable of Object. False when Object is None or has no such number variable. */
	UFUNCTION(BlueprintPure, Category = "DataRead", meta = (ReturnDisplayName = "Found"))
	static bool ReadNumber(const UObject* Object, FName Variable, float& OutValue);

	/** Reads the object variable Variable of Object (a curve, a sub-profile...). None when Object is None or has no such object variable. */
	UFUNCTION(BlueprintPure, Category = "DataRead")
	static UObject* ReadObject(const UObject* Object, FName Variable);

	/** Reads the Gameplay Tag variable Variable of Object. False (empty tag) when Object is None or has no such Gameplay Tag variable. */
	UFUNCTION(BlueprintPure, Category = "DataRead", meta = (ReturnDisplayName = "Found"))
	static bool ReadGameplayTag(const UObject* Object, FName Variable, FGameplayTag& OutTag);

	/**
	 * Calls the function Function of Object (a Blueprint function or interface function without input, e.g. GetPhysicsProfile) and returns
	 * its first object output. None when Object is None, has no such function, or the function returns no object.
	 */
	UFUNCTION(BlueprintCallable, Category = "DataRead")
	static UObject* CallObjectGetter(UObject* Object, FName Function);
};
