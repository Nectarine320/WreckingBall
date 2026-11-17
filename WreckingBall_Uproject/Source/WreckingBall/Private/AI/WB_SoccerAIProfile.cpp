#include "AI/WB_SoccerAIProfile.h"

UWB_SoccerAIProfile::UWB_SoccerAIProfile()
{
	BallClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Blueprint/Ball/BP_PO_Master_Ball_01.BP_PO_Master_Ball_01_C")));
}
