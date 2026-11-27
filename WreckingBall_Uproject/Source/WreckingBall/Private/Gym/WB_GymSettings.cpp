#include "Gym/WB_GymSettings.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBGymSettings, Log, All);

AWB_GymSettings::AWB_GymSettings()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SetHidden(true);
}

AWB_GymSettings* AWB_GymSettings::FindGymSettings(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AWB_GymSettings* Found = nullptr;
	for (TActorIterator<AWB_GymSettings> It(World); It; ++It)
	{
		if (!Found)
		{
			Found = *It;
			continue;
		}
		UE_LOG(LogWBGymSettings, Warning, TEXT("Several BP_GymSettings in the world: %s is used, %s is ignored."), *Found->GetName(), *It->GetName());
	}
	return Found;
}
