#include "Impact/Destructible/WB_DestructionSwitchSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBDestructionSwitch, Log, All);

void UWB_DestructionSwitchSubsystem::SetDestructionEnabled(bool bEnabled)
{
	if (bDestructionEnabled != bEnabled)
	{
		UE_LOG(LogWBDestructionSwitch, Log, TEXT("Destruction %s."), bEnabled ? TEXT("on") : TEXT("off"));
	}
	bDestructionEnabled = bEnabled;
}

bool UWB_DestructionSwitchSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
