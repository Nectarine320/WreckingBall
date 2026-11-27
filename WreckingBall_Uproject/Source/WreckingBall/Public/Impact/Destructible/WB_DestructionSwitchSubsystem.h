#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WB_DestructionSwitchSubsystem.generated.h"

/**
 * World switch of the destruction (devis gym-camera-p1, lot 2). On by default: the match is unchanged.
 * Off (gyms): BP_Destructible_Base refuses every impact (ApplyImpact returns false), so nothing takes damage or breaks.
 * Read and set it with UWB_DestructibleLibrary::IsDestructionEnabled / SetDestructionEnabled.
 */
UCLASS()
class WRECKINGBALL_API UWB_DestructionSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** True while the destructibles of this world take damage. */
	bool IsDestructionEnabled() const { return bDestructionEnabled; }

	/** Turns the destruction of this world on or off. */
	void SetDestructionEnabled(bool bEnabled);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool bDestructionEnabled = true;
};
