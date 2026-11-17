#include "AI/WB_SoccerBotManagerComponent.h"

#include "AI/WB_SoccerAIController_Base.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBSoccerBots, Log, All);

UWB_SoccerBotManagerComponent::UWB_SoccerBotManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	BotControllerClass = AWB_SoccerAIController_Base::StaticClass();
}

void UWB_SoccerBotManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 i = BotLinks.Num() - 1; i >= 0; --i)
	{
		ReleaseBot(i, false);
	}
	Super::EndPlay(EndPlayReason);
}

int32 UWB_SoccerBotManagerComponent::GetNumLocalPlayers() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetNumLocalPlayers() : 0;
}

int32 UWB_SoccerBotManagerComponent::GetBotCount() const
{
	int32 Count = 0;
	for (const FBotLink& Link : BotLinks)
	{
		Count += Link.Bot.IsValid() && Link.Bot->GetPawn() ? 1 : 0;
	}
	return Count;
}

bool UWB_SoccerBotManagerComponent::HasGamepadAssigned(const APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}
	const IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	TArray<FInputDeviceId> Devices;
	Mapper.GetAllInputDevicesForUser(PlayerController->GetPlatformUserId(), Devices);
	for (const FInputDeviceId& Device : Devices)
	{
		if (Mapper.GetInputDeviceConnectionState(Device) == EInputDeviceConnectionState::Connected)
		{
			return true;
		}
	}
	return false;
}

int32 UWB_SoccerBotManagerComponent::AddLocalPlayer()
{
	const int32 Count = GetNumLocalPlayers();
	if (Count >= MaxPlayers)
	{
		ShowMessage(FString::Printf(TEXT("Players: %d (max %d)"), Count, MaxPlayers));
		return -1;
	}

	APlayerController* PlayerController = UGameplayStatics::CreatePlayer(this, -1, true);
	if (!PlayerController)
	{
		UE_LOG(LogWBSoccerBots, Warning, TEXT("AddLocalPlayer: CreatePlayer failed."));
		return -1;
	}

	const int32 PlayerIndex = GetNumLocalPlayers() - 1;
	APawn* Pawn = PlayerController->GetPawn();
	if (Pawn)
	{
		PlaceNewPawn(Pawn, PlayerIndex);
	}
	else
	{
		UE_LOG(LogWBSoccerBots, Warning, TEXT("AddLocalPlayer: player %d has no pawn (no Default Pawn Class?)."), PlayerIndex);
	}

	// Registered by the GameMode (team, material) while the PlayerController still owns the pawn.
	OnLocalPlayerAdded.Broadcast(PlayerIndex, Pawn);

	const bool bBot = bBotsActive && !HasGamepadAssigned(PlayerController) && PossessWithBot(PlayerController);
	ShowMessage(FString::Printf(TEXT("Player %d added%s (%d/%d)"), PlayerIndex + 1, bBot ? TEXT(" as bot") : TEXT(""), PlayerIndex + 1, MaxPlayers));
	return PlayerIndex;
}

bool UWB_SoccerBotManagerComponent::RemoveLastLocalPlayer()
{
	const int32 Count = GetNumLocalPlayers();
	if (Count <= MinPlayers)
	{
		ShowMessage(FString::Printf(TEXT("Players: %d (min %d)"), Count, MinPlayers));
		return false;
	}

	const int32 PlayerIndex = Count - 1;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, PlayerIndex);
	if (!PlayerController)
	{
		return false;
	}

	// The pawn may be driven by a bot: take it from the bot first.
	APawn* Pawn = PlayerController->GetPawn();
	const int32 LinkIndex = FindBotLink(PlayerController);
	if (LinkIndex != INDEX_NONE)
	{
		if (BotLinks[LinkIndex].Bot.IsValid())
		{
			Pawn = BotLinks[LinkIndex].Bot->GetPawn();
		}
		ReleaseBot(LinkIndex, false);
	}

	OnLocalPlayerRemoving.Broadcast(PlayerIndex, Pawn);
	if (Pawn)
	{
		Pawn->Destroy();
	}
	UGameplayStatics::RemovePlayer(PlayerController, true);

	ShowMessage(FString::Printf(TEXT("Player %d removed (%d/%d)"), PlayerIndex + 1, PlayerIndex, MaxPlayers));
	return true;
}

bool UWB_SoccerBotManagerComponent::ToggleBots()
{
	SetBotsActive(!bBotsActive);
	return bBotsActive;
}

void UWB_SoccerBotManagerComponent::SetBotsActive(bool bActive)
{
	bBotsActive = bActive;

	if (bActive)
	{
		// Player 0 (keyboard) always stays human.
		for (int32 i = 1; i < GetNumLocalPlayers(); ++i)
		{
			APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, i);
			if (PlayerController && FindBotLink(PlayerController) == INDEX_NONE && !HasGamepadAssigned(PlayerController))
			{
				PossessWithBot(PlayerController);
			}
		}
	}
	else
	{
		for (int32 i = BotLinks.Num() - 1; i >= 0; --i)
		{
			ReleaseBot(i, true);
		}
	}

	const int32 BotCount = GetBotCount();
	ShowMessage(FString::Printf(TEXT("Bots %s (%d)"), bActive ? TEXT("ON") : TEXT("OFF"), BotCount));
	OnBotsToggled.Broadcast(bActive, BotCount);
}

bool UWB_SoccerBotManagerComponent::PossessWithBot(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !Pawn)
	{
		return false;
	}

	UClass* BotClass = BotControllerClass ? BotControllerClass.Get() : AWB_SoccerAIController_Base::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWB_SoccerAIController_Base* Bot = World->SpawnActor<AWB_SoccerAIController_Base>(BotClass, Pawn->GetActorLocation(), Pawn->GetActorRotation(), Params);
	if (!Bot)
	{
		return false;
	}

	PlayerController->UnPossess();
	Bot->Possess(Pawn);
	BotLinks.Add({ PlayerController, Bot });
	return true;
}

void UWB_SoccerBotManagerComponent::ReleaseBot(int32 Index, bool bReturnPawn)
{
	if (!BotLinks.IsValidIndex(Index))
	{
		return;
	}
	const FBotLink Link = BotLinks[Index];
	BotLinks.RemoveAt(Index);

	APawn* Pawn = nullptr;
	if (AWB_SoccerAIController_Base* Bot = Link.Bot.Get())
	{
		Pawn = Bot->GetPawn();
		Bot->UnPossess();
		Bot->Destroy();
	}
	if (bReturnPawn && Pawn && Link.PlayerController.IsValid())
	{
		Link.PlayerController->Possess(Pawn);
	}
}

int32 UWB_SoccerBotManagerComponent::FindBotLink(const APlayerController* PlayerController) const
{
	return BotLinks.IndexOfByPredicate([PlayerController](const FBotLink& Link) { return Link.PlayerController.Get() == PlayerController; });
}

void UWB_SoccerBotManagerComponent::PlaceNewPawn(APawn* Pawn, int32 PlayerIndex) const
{
	UWorld* World = GetWorld();

	// A PlayerStart is free when no other pawn stands on it.
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		const FVector StartLocation = It->GetActorLocation();
		bool bFree = true;
		for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
		{
			if (*PawnIt != Pawn && FVector::Dist2D(PawnIt->GetActorLocation(), StartLocation) < PlayerStartClearance)
			{
				bFree = false;
				break;
			}
		}
		if (bFree)
		{
			Pawn->TeleportTo(StartLocation, FRotator(0.f, It->GetActorRotation().Yaw, 0.f));
			return;
		}
	}

	// No free PlayerStart: beside player 1, alternating right and left.
	const APawn* FirstPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!FirstPawn || FirstPawn == Pawn)
	{
		return;
	}
	const float Side = (PlayerIndex % 2 == 0) ? 1.f : -1.f;
	const float Rank = static_cast<float>(FMath::Max(1, PlayerIndex / 2));
	FVector Location = FirstPawn->GetActorLocation() + FirstPawn->GetActorRightVector() * Side * Rank * SpawnBesideDistance;

	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Location, Projected, FVector(150.f, 150.f, 300.f)))
		{
			// Keep the capsule height of player 1 above the projected floor point.
			Location.X = Projected.Location.X;
			Location.Y = Projected.Location.Y;
		}
	}
	Pawn->TeleportTo(Location, FRotator(0.f, FirstPawn->GetActorRotation().Yaw, 0.f));
}

void UWB_SoccerBotManagerComponent::ShowMessage(const FString& Message) const
{
	UE_LOG(LogWBSoccerBots, Log, TEXT("%s"), *Message);
	if (bShowMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, Message);
	}
}
