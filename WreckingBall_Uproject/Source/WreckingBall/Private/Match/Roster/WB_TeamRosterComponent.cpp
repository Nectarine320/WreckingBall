#include "Match/Roster/WB_TeamRosterComponent.h"

#include "AI/WB_SoccerAIController_Base.h"
#include "Ball/Dribble/WB_BallDribbleComponent.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Kismet/GameplayStatics.h"
#include "Match/Roster/WB_TeamRosterProfile.h"
#include "Match/Roster/WB_TeamSpawnPoint.h"
#include "Match/WB_SoccerPlayerState.h"
#include "Match/WB_TeamGoalComponent.h"
#include "Match/WB_TeamLibrary.h"
#include "NavigationSystem.h"
#include "Player/Command/WB_PlayerCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBTeamRoster, Log, All);

namespace
{
	/** The match always has two teams: 0 = red, 1 = blue. */
	constexpr int32 NumTeams = 2;

	/** Local players: P1 to P4. */
	constexpr int32 MaxLocalPlayers = 4;

	/** Height in cm above the floor where a character is put back, so the capsule does not start inside the floor. Technical value. */
	constexpr float PlaceLift = 2.f;
}

UWB_TeamRosterComponent::UWB_TeamRosterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_TeamRosterComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWB_BallPossessionSubsystem* Possession = GetWorld() ? GetWorld()->GetSubsystem<UWB_BallPossessionSubsystem>() : nullptr)
	{
		Possession->OnBallPossessionChanged.AddDynamic(this, &UWB_TeamRosterComponent::HandleBallPossessionChanged);
	}
}

void UWB_TeamRosterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWB_BallPossessionSubsystem* Possession = GetWorld() ? GetWorld()->GetSubsystem<UWB_BallPossessionSubsystem>() : nullptr)
	{
		Possession->OnBallPossessionChanged.RemoveDynamic(this, &UWB_TeamRosterComponent::HandleBallPossessionChanged);
	}
	PendingPasses.Reset();
	Super::EndPlay(EndPlayReason);
}

UWB_TeamRosterComponent* UWB_TeamRosterComponent::GetTeamRoster(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	return GameMode ? GameMode->FindComponentByClass<UWB_TeamRosterComponent>() : nullptr;
}

bool UWB_TeamRosterComponent::HasProfile() const
{
	if (!RosterProfile && !bWarnedNoProfile)
	{
		bWarnedNoProfile = true;
		UE_LOG(LogWBTeamRoster, Warning, TEXT("%s: no RosterProfile (DA_TeamRoster_Default expected). The 3v3 roster and the switch / pass are off."), *GetPathNameSafe(this));
	}
	return RosterProfile != nullptr;
}

// ---- Setup ----

int32 UWB_TeamRosterComponent::CreateGamepadPlayers()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return 0;
	}

	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	int32 Created = 0;
	// Controller 0 is P1 (gamepad 1 + keyboard), always there. Gamepad N+1 drives controller N.
	for (int32 ControllerId = 1; ControllerId < MaxLocalPlayers; ++ControllerId)
	{
		if (GameInstance->FindLocalPlayerFromControllerId(ControllerId))
		{
			continue;
		}
		TArray<FInputDeviceId> Devices;
		Mapper.GetAllInputDevicesForUser(Mapper.GetPlatformUserForUserIndex(ControllerId), Devices);
		const bool bConnected = Devices.ContainsByPredicate([&Mapper](const FInputDeviceId& Device)
		{
			return Mapper.GetInputDeviceConnectionState(Device) == EInputDeviceConnectionState::Connected;
		});
		if (bConnected && UGameplayStatics::CreatePlayer(this, ControllerId, true))
		{
			++Created;
		}
	}
	UE_LOG(LogWBTeamRoster, Log, TEXT("CreateGamepadPlayers: %d player(s) created, %d local player(s)."), Created, GameInstance->GetNumLocalPlayers());
	return Created;
}

bool UWB_TeamRosterComponent::BuildRoster()
{
	UWorld* World = GetWorld();
	if (!World || !HasProfile())
	{
		return false;
	}
	if (bBuilt)
	{
		UE_LOG(LogWBTeamRoster, Warning, TEXT("BuildRoster: the roster is already built."));
		return true;
	}

	const int32 PerTeam = RosterProfile->PlayersPerTeam;
	bBotsActive = RosterProfile->bStartBotsOnPlay;
	Entries.Reset();

	AWB_TeamSpawnPoint* SpawnPoints[NumTeams] = { nullptr, nullptr };
	FindSpawnPoints(SpawnPoints[0], SpawnPoints[1]);

	// Humans first, in ID order: they take the first places of their team.
	int32 Taken[NumTeams] = { 0, 0 };
	const int32 NumLocal = World->GetGameInstance() ? World->GetGameInstance()->GetNumLocalPlayers() : 0;
	for (int32 PlayerIndex = 0; PlayerIndex < NumLocal; ++PlayerIndex)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, PlayerIndex);
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		const int32 Team = UWB_TeamLibrary::GetTeamOf(PlayerController);
		if (!Pawn || Team < 0 || Team >= NumTeams)
		{
			UE_LOG(LogWBTeamRoster, Warning, TEXT("BuildRoster: player %d has no pawn or no team (%d), left out of the roster."), PlayerIndex + 1, Team);
			continue;
		}
		if (Taken[Team] >= PerTeam)
		{
			UE_LOG(LogWBTeamRoster, Warning, TEXT("BuildRoster: team %d is full (%d places), player %d left out of the roster."), Team, PerTeam, PlayerIndex + 1);
			continue;
		}
		Entries.Add({ Pawn, Team, Taken[Team]++ });
	}

	// Bots on the free places.
	for (int32 Team = 0; Team < NumTeams; ++Team)
	{
		for (int32 Place = Taken[Team]; Place < PerTeam; ++Place)
		{
			if (APawn* Bot = SpawnBotCharacter(Team, Place, SpawnPoints[Team]))
			{
				Entries.Add({ Bot, Team, Place });
			}
		}
	}

	Entries.StableSort([](const FRosterEntry& A, const FRosterEntry& B)
	{
		return A.TeamIndex != B.TeamIndex ? A.TeamIndex < B.TeamIndex : A.PlaceIndex < B.PlaceIndex;
	});
	bBuilt = true;

	for (const FRosterEntry& Entry : Entries)
	{
		PlaceCharacter(Entry.Character.Get(), Entry.TeamIndex, Entry.PlaceIndex, SpawnPoints[Entry.TeamIndex]);
	}
	for (const FRosterEntry& Entry : Entries)
	{
		OnRosterCharacterAdded.Broadcast(Entry.Character.Get(), Entry.TeamIndex, Entry.PlaceIndex);
	}

	UE_LOG(LogWBTeamRoster, Log, TEXT("BuildRoster: %d characters (%d per team), %d bot(s), bots %s."), Entries.Num(), PerTeam, GetBotCount(), bBotsActive ? TEXT("on") : TEXT("off"));
	return true;
}

AWB_SoccerAIController_Base* UWB_TeamRosterComponent::SpawnBot(int32 TeamIndex, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	if (!World || !BotControllerClass)
	{
		UE_LOG(LogWBTeamRoster, Warning, TEXT("%s: no BotControllerClass (BP_SoccerAIController expected), no bot spawned."), *GetPathNameSafe(this));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWB_SoccerAIController_Base* Bot = World->SpawnActor<AWB_SoccerAIController_Base>(BotControllerClass, Location, Rotation, Params);
	if (!Bot)
	{
		return nullptr;
	}

	// The team lives in the PlayerState, for the bots too (single source, read by UWB_TeamLibrary::GetTeamOf).
	if (!Bot->GetPlayerState<APlayerState>())
	{
		Bot->InitPlayerState();
	}
	if (AWB_SoccerPlayerState* PlayerState = Bot->GetPlayerState<AWB_SoccerPlayerState>())
	{
		PlayerState->SetPlayerName(FString::Printf(TEXT("COM %d"), ++SpawnedBotCount));
		PlayerState->SetTeam(TeamIndex);
	}
	else
	{
		UE_LOG(LogWBTeamRoster, Warning, TEXT("%s: the bot has no AWB_SoccerPlayerState (Player State Class of the GameMode?), its team is unknown."), *Bot->GetName());
	}
	Bot->SetBrainPaused(!bBotsActive);
	return Bot;
}

APawn* UWB_TeamRosterComponent::SpawnBotCharacter(int32 TeamIndex, int32 PlaceIndex, AWB_TeamSpawnPoint* SpawnPoint)
{
	UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	UClass* PawnClass = GameMode ? GameMode->DefaultPawnClass.Get() : nullptr;
	if (!PawnClass)
	{
		UE_LOG(LogWBTeamRoster, Warning, TEXT("SpawnBotCharacter: the GameMode has no Default Pawn Class, no bot character spawned."));
		return nullptr;
	}

	FVector Floor;
	FRotator Rotation;
	ComputePlace(TeamIndex, PlaceIndex, SpawnPoint, Floor, Rotation);
	// Spawned a bit above its place, then put exactly on it by PlaceCharacter.
	const FTransform Transform(Rotation, Floor + FVector(0.f, 0.f, 100.f));

	APawn* Pawn = World->SpawnActorDeferred<APawn>(PawnClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Pawn)
	{
		return nullptr;
	}
	// The roster gives the controller itself.
	Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Pawn->FinishSpawning(Transform);

	AWB_SoccerAIController_Base* Bot = SpawnBot(TeamIndex, Transform.GetLocation(), Rotation);
	if (!Bot)
	{
		Pawn->Destroy();
		return nullptr;
	}
	Bot->Possess(Pawn);
	return Pawn;
}

// ---- Places ----

void UWB_TeamRosterComponent::FindSpawnPoints(AWB_TeamSpawnPoint*& OutRed, AWB_TeamSpawnPoint*& OutBlue) const
{
	AWB_TeamSpawnPoint* Found[NumTeams] = { nullptr, nullptr };
	const int32 PerTeam = RosterProfile ? RosterProfile->PlayersPerTeam : 0;
	for (int32 Team = 0; Team < NumTeams; ++Team)
	{
		Found[Team] = AWB_TeamSpawnPoint::FindForTeam(this, Team);
		if (!Found[Team])
		{
			UE_LOG(LogWBTeamRoster, Warning, TEXT("No BP_TeamSpawnPoint for team %d: its characters appear side by side around (0, 0, 0)."), Team);
		}
		else if (Found[Team]->GetPlaceCount() < PerTeam)
		{
			UE_LOG(LogWBTeamRoster, Warning, TEXT("%s has %d places for %d characters: the others appear around (0, 0, 0)."), *Found[Team]->GetName(), Found[Team]->GetPlaceCount(), PerTeam);
		}
	}
	OutRed = Found[0];
	OutBlue = Found[1];
}

void UWB_TeamRosterComponent::ComputePlace(int32 TeamIndex, int32 PlaceIndex, const AWB_TeamSpawnPoint* SpawnPoint, FVector& OutFloor, FRotator& OutRotation) const
{
	FTransform Place;
	if (SpawnPoint && SpawnPoint->GetPlaceTransform(PlaceIndex, Place))
	{
		OutFloor = Place.GetLocation();
		OutRotation = FRotator(0.f, Place.Rotator().Yaw, 0.f);
		return;
	}
	ComputeFallbackPlace(TeamIndex, PlaceIndex, OutFloor, OutRotation);
}

void UWB_TeamRosterComponent::ComputeFallbackPlace(int32 TeamIndex, int32 PlaceIndex, FVector& OutFloor, FRotator& OutRotation) const
{
	if (!HasProfile())
	{
		OutFloor = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}
	const float Radius = RosterProfile->FallbackRadius;
	const float Spacing = RosterProfile->FallbackSpacing;
	const int32 PerTeam = RosterProfile->PlayersPerTeam;

	// The team stands on its own half: opposite to the goal it attacks (red on -X, blue on +X without goals).
	FVector Side = TeamIndex == 0 ? -FVector::ForwardVector : FVector::ForwardVector;
	if (const UWB_TeamGoalComponent* Goal = UWB_TeamGoalComponent::FindGoalToAttack(this, TeamIndex, FVector::ZeroVector))
	{
		FVector AwayFromGoal = -Goal->GetAimLocation();
		AwayFromGoal.Z = 0.f;
		if (AwayFromGoal.Normalize())
		{
			Side = AwayFromGoal;
		}
	}

	// Side by side along the arc, centered on the team's side.
	const float Angle = FMath::Atan2(Side.Y, Side.X) + (PlaceIndex - (PerTeam - 1) * 0.5f) * (Spacing / FMath::Max(Radius, 1.f));
	FVector Point(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);

	bool bOnFloor = false;
	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		// Search box: half the spacing sideways (neighbours stay apart), tall enough for a floor that is not at Z = 0. Technical value.
		if (NavSys->ProjectPointToNavigation(Point, Projected, FVector(Spacing * 0.5f, Spacing * 0.5f, 2000.f)))
		{
			Point = Projected.Location;
			bOnFloor = true;
		}
	}
	if (!bOnFloor)
	{
		// No NavMesh there: the floor below the point (Visibility, the gym floors are WorldDynamic). Search height: technical value.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WBRosterFallbackFloor), false);
		for (const APawn* Character : GetRosterCharacters())
		{
			Params.AddIgnoredActor(Character);
		}
		if (GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 2000.f), Point - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			Point = Hit.ImpactPoint;
		}
	}
	OutFloor = Point;
	OutRotation = FRotator(0.f, (-FVector(Point.X, Point.Y, 0.f)).Rotation().Yaw, 0.f);
}

void UWB_TeamRosterComponent::PlaceCharacter(APawn* Character, int32 TeamIndex, int32 PlaceIndex, const AWB_TeamSpawnPoint* SpawnPoint) const
{
	if (!Character)
	{
		return;
	}
	FVector Floor;
	FRotator Rotation;
	ComputePlace(TeamIndex, PlaceIndex, SpawnPoint, Floor, Rotation);

	ACharacter* AsCharacter = Cast<ACharacter>(Character);
	const float HalfHeight = AsCharacter && AsCharacter->GetCapsuleComponent() ? AsCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	Character->TeleportTo(Floor + FVector(0.f, 0.f, HalfHeight + PlaceLift), Rotation, false, true);
	if (AsCharacter && AsCharacter->GetCharacterMovement())
	{
		AsCharacter->GetCharacterMovement()->StopMovementImmediately();
	}
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(Rotation);
	}
}

void UWB_TeamRosterComponent::ResetCharactersToPlaces()
{
	if (!bBuilt)
	{
		return;
	}
	AWB_TeamSpawnPoint* SpawnPoints[NumTeams] = { nullptr, nullptr };
	FindSpawnPoints(SpawnPoints[0], SpawnPoints[1]);
	for (const FRosterEntry& Entry : Entries)
	{
		APawn* Character = Entry.Character.Get();
		PlaceCharacter(Character, Entry.TeamIndex, Entry.PlaceIndex, SpawnPoints[Entry.TeamIndex]);
		// After a goal, every bot may Dash on a player again at once.
		if (AWB_SoccerAIController_Base* Bot = Character ? Cast<AWB_SoccerAIController_Base>(Character->GetController()) : nullptr)
		{
			Bot->ResetDashCooldown();
		}
	}
}

// ---- Switch and pass ----

bool UWB_TeamRosterComponent::SwitchOrPass(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !Pawn || !bBuilt || !HasProfile())
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (const double* Last = LastSwitchTimes.Find(PlayerController); Last && Now - *Last < RosterProfile->SwitchCooldown)
	{
		return false;
	}

	// A pass of this player is on its way: a second press takes the receiver at once.
	const int32 PendingIndex = PendingPasses.IndexOfByPredicate([PlayerController](const FPendingPass& Pass) { return Pass.PlayerController.Get() == PlayerController; });
	if (PendingIndex != INDEX_NONE)
	{
		APawn* Receiver = PendingPasses[PendingIndex].Receiver.Get();
		PendingPasses.RemoveAt(PendingIndex);
		if (Receiver && Cast<AWB_SoccerAIController_Base>(Receiver->GetController()) && SwitchToCharacter(PlayerController, Receiver))
		{
			LastSwitchTimes.Add(PlayerController, Now);
			return true;
		}
	}

	// World direction of the stick (last movement input), zero at rest.
	const FVector Direction = Pawn->GetLastMovementInputVector();
	UWB_PlayerCommandComponent* Commands = Pawn->FindComponentByClass<UWB_PlayerCommandComponent>();

	if (Commands && Commands->IsHoldingBall())
	{
		// Charged pass (UWB_PassProfile::bUseChargedPass): the press starts the charge, ReleaseSwitchOrPass sends the pass.
		if (Commands->BeginPassCharge())
		{
			return true;
		}
		return PassToward(PlayerController, Pawn, Commands, Direction, 0.f);
	}

	APawn* Target = UWB_TeamLibrary::FindDirectionalAlly(Pawn, Direction, true);
	if (!Target || !SwitchToCharacter(PlayerController, Target))
	{
		return false;
	}
	LastSwitchTimes.Add(PlayerController, Now);
	return true;
}

bool UWB_TeamRosterComponent::ReleaseSwitchOrPass(APlayerController* PlayerController)
{
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWB_PlayerCommandComponent* Commands = Pawn ? Pawn->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
	if (!Commands || !Commands->IsChargingPass())
	{
		return false;
	}
	const float Charge = Commands->GetPassCharge();
	Commands->CancelPassCharge();
	// The ball was lost during the charge (Dash, steal): nothing to pass.
	if (!bBuilt || !HasProfile() || !Commands->IsHoldingBall())
	{
		return false;
	}
	return PassToward(PlayerController, Pawn, Commands, Pawn->GetLastMovementInputVector(), Charge);
}

bool UWB_TeamRosterComponent::PassToward(APlayerController* PlayerController, APawn* Pawn, UWB_PlayerCommandComponent* Commands, const FVector& Direction, float Charge)
{
	const UWB_BallDribbleComponent* Dribble = Pawn->FindComponentByClass<UWB_BallDribbleComponent>();
	AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;
	// Aim assist and free pass: see UWB_PassProfile::bUseAimAssist. Receiver is None after a free pass.
	APawn* Receiver = nullptr;
	if (!Commands->RequestDirectionalPass(Direction, Receiver, Charge))
	{
		return false;
	}
	LastSwitchTimes.Add(PlayerController, GetWorld()->GetTimeSeconds());
	// The passer takes the receiver when it catches the ball (HandleBallPossessionChanged). A receiver driven by another human only gets the ball.
	if (Ball && Receiver && Cast<AWB_SoccerAIController_Base>(Receiver->GetController()))
	{
		PendingPasses.Add({ PlayerController, Receiver, Ball });
	}
	return true;
}

bool UWB_TeamRosterComponent::SwitchToCharacter(APlayerController* PlayerController, APawn* Target)
{
	if (!PlayerController || !Target || PlayerController->GetPawn() == Target || !UWB_TeamLibrary::AreTeammates(PlayerController, Target))
	{
		return false;
	}
	AWB_SoccerAIController_Base* Bot = Cast<AWB_SoccerAIController_Base>(Target->GetController());
	if (!Bot)
	{
		return false;
	}

	// The two controllers swap characters: each keeps its PlayerState (team, P1-P4 label), each character keeps its state.
	APawn* OldCharacter = PlayerController->GetPawn();
	// The view stays on the shared match camera: possessing a pawn would otherwise move the view target to it.
	AActor* ViewTarget = PlayerController->GetViewTarget();
	Bot->UnPossess();
	PlayerController->Possess(Target);
	if (ViewTarget && ViewTarget != OldCharacter)
	{
		PlayerController->SetViewTarget(ViewTarget);
	}
	if (OldCharacter)
	{
		Bot->Possess(OldCharacter);
		Bot->SetBrainPaused(!bBotsActive);
	}
	else
	{
		Bot->Destroy();
	}
	OnCharacterSwitched.Broadcast(PlayerController, OldCharacter, Target);
	return true;
}

void UWB_TeamRosterComponent::HandleBallPossessionChanged(AActor* Ball, AActor* NewPossessor, AActor* OldPossessor)
{
	for (int32 i = PendingPasses.Num() - 1; i >= 0; --i)
	{
		const FPendingPass Pass = PendingPasses[i];
		if (!Pass.Ball.IsValid() || !Pass.PlayerController.IsValid())
		{
			PendingPasses.RemoveAt(i);
			continue;
		}
		// The ball leaving the passer's feet fires with no new possessor: the pass is still travelling.
		if (Pass.Ball.Get() != Ball || !NewPossessor)
		{
			continue;
		}
		PendingPasses.RemoveAt(i);
		APawn* Receiver = Pass.Receiver.Get();
		if (Receiver && NewPossessor == Receiver && Cast<AWB_SoccerAIController_Base>(Receiver->GetController()))
		{
			SwitchToCharacter(Pass.PlayerController.Get(), Receiver);
		}
	}

	// A roster character took the ball: remember a human holder, or give a bot holder to a human of its team.
	APawn* Holder = Cast<APawn>(NewPossessor);
	if (!Holder || !bBuilt || !RosterProfile || !FindEntry(Holder))
	{
		return;
	}
	const int32 Team = UWB_TeamLibrary::GetTeamOf(Holder);
	if (APlayerController* HumanHolder = Cast<APlayerController>(Holder->GetController()))
	{
		LastHumanHolders.Add(Team, HumanHolder);
		return;
	}
	if (RosterProfile->bTakeBotCarrier && Cast<AWB_SoccerAIController_Base>(Holder->GetController()))
	{
		if (APlayerController* Human = FindHumanForTeam(Team, Holder))
		{
			SwitchToCharacter(Human, Holder);
			LastHumanHolders.Add(Team, Human);
		}
	}
}

APlayerController* UWB_TeamRosterComponent::FindHumanForTeam(int32 TeamIndex, const APawn* Near) const
{
	if (TeamIndex < 0)
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<APlayerController>* Last = LastHumanHolders.Find(TeamIndex))
	{
		APlayerController* PlayerController = Last->Get();
		if (PlayerController && PlayerController->GetPawn() && UWB_TeamLibrary::GetTeamOf(PlayerController) == TeamIndex)
		{
			return PlayerController;
		}
	}

	APlayerController* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const int32 NumLocal = GameInstance ? GameInstance->GetNumLocalPlayers() : 0;
	for (int32 i = 0; i < NumLocal; ++i)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, i);
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Pawn || UWB_TeamLibrary::GetTeamOf(PlayerController) != TeamIndex)
		{
			continue;
		}
		const double Distance = Near ? FVector::DistSquared(Pawn->GetActorLocation(), Near->GetActorLocation()) : 0.0;
		if (Distance < BestDistance)
		{
			Best = PlayerController;
			BestDistance = Distance;
		}
	}
	return Best;
}

// ---- Bots ----

bool UWB_TeamRosterComponent::ToggleBots()
{
	SetBotsActive(!bBotsActive);
	return bBotsActive;
}

void UWB_TeamRosterComponent::SetBotsActive(bool bActive)
{
	bBotsActive = bActive;
	for (const FRosterEntry& Entry : Entries)
	{
		const APawn* Character = Entry.Character.Get();
		if (AWB_SoccerAIController_Base* Bot = Character ? Cast<AWB_SoccerAIController_Base>(Character->GetController()) : nullptr)
		{
			Bot->SetBrainPaused(!bActive);
		}
	}
	const int32 BotCount = GetBotCount();
	ShowMessage(FString::Printf(TEXT("Bots %s (%d)"), bActive ? TEXT("ON") : TEXT("OFF"), BotCount));
	OnBotsToggled.Broadcast(bActive, BotCount);
}

int32 UWB_TeamRosterComponent::GetBotCount() const
{
	int32 Count = 0;
	for (const FRosterEntry& Entry : Entries)
	{
		const APawn* Character = Entry.Character.Get();
		Count += Character && Cast<AWB_SoccerAIController_Base>(Character->GetController()) ? 1 : 0;
	}
	return Count;
}

// ---- Players joining and leaving ----

bool UWB_TeamRosterComponent::AddHumanCharacter(APlayerController* PlayerController)
{
	if (!bBuilt || !PlayerController)
	{
		return false;
	}
	APawn* SparePawn = PlayerController->GetPawn();
	if (SparePawn && FindEntry(SparePawn))
	{
		return true;
	}
	const int32 Team = UWB_TeamLibrary::GetTeamOf(PlayerController);

	// The bot character with the highest place of the team.
	const FRosterEntry* Best = nullptr;
	for (const FRosterEntry& Entry : Entries)
	{
		const APawn* Character = Entry.Character.Get();
		if (Entry.TeamIndex == Team && Character && Cast<AWB_SoccerAIController_Base>(Character->GetController()) && (!Best || Entry.PlaceIndex > Best->PlaceIndex))
		{
			Best = &Entry;
		}
	}
	if (!Best)
	{
		UE_LOG(LogWBTeamRoster, Warning, TEXT("AddHumanCharacter: team %d has no bot character to give to %s."), Team, *PlayerController->GetName());
		return false;
	}

	APawn* Target = Best->Character.Get();
	AController* Bot = Target->GetController();
	Bot->UnPossess();
	Bot->Destroy();
	if (SparePawn)
	{
		OnRosterCharacterRemoving.Broadcast(SparePawn);
		SparePawn->Destroy();
	}
	PlayerController->Possess(Target);
	return true;
}

bool UWB_TeamRosterComponent::ReleaseHumanCharacter(APawn* Character)
{
	const FRosterEntry* Entry = FindEntry(Character);
	if (!Entry)
	{
		return false;
	}
	AWB_SoccerAIController_Base* Bot = SpawnBot(Entry->TeamIndex, Character->GetActorLocation(), Character->GetActorRotation());
	if (!Bot)
	{
		return false;
	}
	if (AController* Previous = Character->GetController())
	{
		Previous->UnPossess();
	}
	Bot->Possess(Character);
	return true;
}

// ---- Queries ----

TArray<APawn*> UWB_TeamRosterComponent::GetRosterCharacters() const
{
	TArray<APawn*> Characters;
	for (const FRosterEntry& Entry : Entries)
	{
		if (APawn* Character = Entry.Character.Get())
		{
			Characters.Add(Character);
		}
	}
	return Characters;
}

bool UWB_TeamRosterComponent::GetCharacterPlace(const APawn* Character, int32& TeamIndex, int32& PlaceIndex) const
{
	const FRosterEntry* Entry = FindEntry(Character);
	TeamIndex = Entry ? Entry->TeamIndex : -1;
	PlaceIndex = Entry ? Entry->PlaceIndex : -1;
	return Entry != nullptr;
}

const UWB_TeamRosterComponent::FRosterEntry* UWB_TeamRosterComponent::FindEntry(const APawn* Character) const
{
	return Character ? Entries.FindByPredicate([Character](const FRosterEntry& Entry) { return Entry.Character.Get() == Character; }) : nullptr;
}

void UWB_TeamRosterComponent::ShowMessage(const FString& Message) const
{
	UE_LOG(LogWBTeamRoster, Log, TEXT("%s"), *Message);
	if (bShowMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, Message);
	}
}
