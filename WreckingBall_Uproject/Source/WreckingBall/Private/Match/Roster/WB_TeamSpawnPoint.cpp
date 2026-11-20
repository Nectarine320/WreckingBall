#include "Match/Roster/WB_TeamSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Match/WB_TeamLibrary.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBTeamSpawn, Log, All);

AWB_TeamSpawnPoint::AWB_TeamSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Default layout: place 1 in front, places 2 and 3 behind on each side, main point ahead. Moved per map in the viewport.
	MainPoint = CreateDefaultSubobject<USceneComponent>(TEXT("MainPoint"));
	MainPoint->SetupAttachment(Root);
	MainPoint->SetRelativeLocation(FVector(250.f, 0.f, 0.f));

	Place1 = CreateDefaultSubobject<USceneComponent>(TEXT("Place1"));
	Place1->SetupAttachment(Root);

	Place2 = CreateDefaultSubobject<USceneComponent>(TEXT("Place2"));
	Place2->SetupAttachment(Root);
	Place2->SetRelativeLocation(FVector(-150.f, -220.f, 0.f));

	Place3 = CreateDefaultSubobject<USceneComponent>(TEXT("Place3"));
	Place3->SetupAttachment(Root);
	Place3->SetRelativeLocation(FVector(-150.f, 220.f, 0.f));

	AddPreview(Place1, TEXT("Place1"), FText::FromString(TEXT("1")));
	AddPreview(Place2, TEXT("Place2"), FText::FromString(TEXT("2")));
	AddPreview(Place3, TEXT("Place3"), FText::FromString(TEXT("3")));
	AddPreview(MainPoint, TEXT("MainPoint"), FText::FromString(TEXT("BALL")));
}

void AWB_TeamSpawnPoint::AddPreview(USceneComponent* Parent, const FName& BaseName, const FText& Label)
{
	UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(*(BaseName.ToString() + TEXT("_Label")));
	Text->SetupAttachment(Parent);
	// Lying flat, readable from the top camera.
	Text->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	Text->SetRelativeRotation(FRotator(90.f, 180.f, 0.f));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetWorldSize(60.f);
	Text->SetText(Label);
	Text->SetHiddenInGame(true);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewLabels.Add(Text);

	UArrowComponent* Arrow = CreateDefaultSubobject<UArrowComponent>(*(BaseName.ToString() + TEXT("_Arrow")));
	Arrow->SetupAttachment(Parent);
	Arrow->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	Arrow->ArrowSize = 1.5f;
	Arrow->SetHiddenInGame(true);
	PreviewArrows.Add(Arrow);
}

void AWB_TeamSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FWB_TeamDefinition Definition;
	UWB_TeamLibrary::GetTeamDefinition(TeamIndex, Definition);
	const FColor Color = Definition.RingColor.ToFColor(true);
	for (UTextRenderComponent* Text : PreviewLabels)
	{
		if (Text)
		{
			Text->SetTextRenderColor(Color);
		}
	}
	for (UArrowComponent* Arrow : PreviewArrows)
	{
		if (Arrow)
		{
			Arrow->SetArrowColor(Definition.RingColor);
		}
	}
}

bool AWB_TeamSpawnPoint::GetPlaceTransform(int32 PlaceIndex, FTransform& OutTransform) const
{
	const USceneComponent* Places[] = { Place1, Place2, Place3 };
	if (PlaceIndex < 0 || PlaceIndex >= UE_ARRAY_COUNT(Places) || !Places[PlaceIndex])
	{
		return false;
	}
	OutTransform = Places[PlaceIndex]->GetComponentTransform();
	return true;
}

AWB_TeamSpawnPoint* AWB_TeamSpawnPoint::FindForTeam(const UObject* WorldContextObject, int32 InTeamIndex)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AWB_TeamSpawnPoint* Found = nullptr;
	for (TActorIterator<AWB_TeamSpawnPoint> It(World); It; ++It)
	{
		if (It->TeamIndex != InTeamIndex)
		{
			continue;
		}
		if (!Found)
		{
			Found = *It;
		}
		else
		{
			UE_LOG(LogWBTeamSpawn, Warning, TEXT("Several team spawn points for team %d (%s, %s): %s is used."), InTeamIndex, *Found->GetName(), *It->GetName(), *Found->GetName());
		}
	}
	return Found;
}

FVector AWB_TeamSpawnPoint::GetKickoffBallLocation(const UObject* WorldContextObject, int32 ConcedingTeamIndex)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return FVector::ZeroVector;
	}
	if (ConcedingTeamIndex >= 0)
	{
		const AWB_TeamSpawnPoint* SpawnPoint = FindForTeam(WorldContextObject, ConcedingTeamIndex);
		if (SpawnPoint && SpawnPoint->bUseMainPointForBall && SpawnPoint->MainPoint)
		{
			return SpawnPoint->MainPoint->GetComponentLocation();
		}
	}

	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation Projected;
		// Search box around the origin, tall enough for a floor that is not at Z = 0. Technical value.
		if (NavSys->ProjectPointToNavigation(FVector::ZeroVector, Projected, FVector(100.f, 100.f, 2000.f)))
		{
			return Projected.Location;
		}
	}
	// No NavMesh: the floor under the origin (Visibility, the gym floors are WorldDynamic). Search height: technical value.
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, FVector(0.f, 0.f, 2000.f), FVector(0.f, 0.f, -2000.f), ECC_Visibility))
	{
		UE_LOG(LogWBTeamSpawn, Warning, TEXT("GetKickoffBallLocation: no NavMesh under the world origin, the ball appears on the floor below it (%s)."), *GetNameSafe(Hit.GetActor()));
		return Hit.ImpactPoint;
	}
	UE_LOG(LogWBTeamSpawn, Warning, TEXT("GetKickoffBallLocation: no NavMesh nor floor under the world origin, the ball appears at (0, 0, 0)."));
	return FVector::ZeroVector;
}
