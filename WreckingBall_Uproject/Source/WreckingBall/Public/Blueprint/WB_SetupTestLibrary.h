#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_SetupTestLibrary.generated.h"

UCLASS()
class WRECKINGBALL_API UWB_SetupTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Fonction de test temporaire : confirme que le module C++ est appelable depuis Blueprint.
	UFUNCTION(BlueprintPure, Category = "Setup|Test")
	static FString GetCppSetupStatus();
};