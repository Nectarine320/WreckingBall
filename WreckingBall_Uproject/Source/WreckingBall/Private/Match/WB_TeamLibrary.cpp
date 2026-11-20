#include "Match/WB_TeamLibrary.h"

#include "AIController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Match/WB_SoccerPlayerState.h"
#include "Player/Hit/WB_PlayerHitComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBTeam, Log, All);

AWB_SoccerPlayerState* UWB_TeamLibrary::GetSoccerPlayerState(const AActor* Actor)
{
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->GetPlayerState<AWB_SoccerPlayerState>();
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		return Controller->GetPlayerState<AWB_SoccerPlayerState>();
	}
	return const_cast<AWB_SoccerPlayerState*>(Cast<AWB_SoccerPlayerState>(Actor));
}

int32 UWB_TeamLibrary::GetTeamOf(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return -1;
	}
	if (const AWB_SoccerPlayerState* PlayerState = GetSoccerPlayerState(Actor))
	{
		return PlayerState->GetTeam();
	}

	// A pawn driven by a bot (no PlayerState), or a bot controller: the copy kept by the pawn.
	const AActor* PawnActor = Actor;
	if (const AController* Controller = Cast<AController>(Actor))
	{
		PawnActor = Controller->GetPawn();
	}
	const UWB_PlayerHitComponent* Hit = PawnActor ? PawnActor->FindComponentByClass<UWB_PlayerHitComponent>() : nullptr;
	return Hit ? Hit->TeamIndex : -1;
}

bool UWB_TeamLibrary::AreTeammates(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	const int32 TeamA = GetTeamOf(A);
	return TeamA >= 0 && TeamA == GetTeamOf(B);
}

bool UWB_TeamLibrary::IsHumanControlled(const APawn* Pawn)
{
	return Pawn && Cast<APlayerController>(Pawn->GetController()) != nullptr;
}

APawn* UWB_TeamLibrary::FindDirectionalAlly(const APawn* From, FVector Direction, bool bOnlyAIControlled)
{
	UWorld* World = From ? From->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	Direction.Z = 0.f;
	// Below this size the stick is at rest. Technical value (input vectors are 0..1).
	const bool bHasDirection = Direction.Normalize(0.01f);
	const FVector FromLocation = From->GetActorLocation();

	APawn* Best = nullptr;
	float BestDot = -2.f;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (Candidate == From || !AreTeammates(From, Candidate))
		{
			continue;
		}
		if (bOnlyAIControlled && !Cast<AAIController>(Candidate->GetController()))
		{
			continue;
		}

		FVector ToCandidate = Candidate->GetActorLocation() - FromLocation;
		ToCandidate.Z = 0.f;
		const float Distance = ToCandidate.Size();
		// Angles closer than this count as a tie (about 3 degrees). Technical value.
		constexpr float DotTie = 0.0015f;
		const float Dot = bHasDirection && Distance > KINDA_SMALL_NUMBER ? FVector::DotProduct(ToCandidate / Distance, Direction) : 1.f;

		const bool bBetter = bHasDirection
			? (Dot > BestDot + DotTie || (FMath::Abs(Dot - BestDot) <= DotTie && Distance < BestDistance))
			: Distance < BestDistance;
		if (bBetter)
		{
			Best = Candidate;
			BestDot = Dot;
			BestDistance = Distance;
		}
	}
	return Best;
}

const UWB_TeamDefinitionData* UWB_TeamLibrary::GetTeamDefinitions()
{
	const UWB_TeamDeveloperSettings* Settings = GetDefault<UWB_TeamDeveloperSettings>();
	const UWB_TeamDefinitionData* Data = Settings ? Settings->TeamDefinitions.LoadSynchronous() : nullptr;
	if (!Data)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogWBTeam, Warning, TEXT("No team definitions: set Project Settings > Game > Wrecking Ball Teams > Team Definitions (DA_TeamDefinitions)."));
		}
	}
	return Data;
}

bool UWB_TeamLibrary::GetTeamDefinition(int32 TeamIndex, FWB_TeamDefinition& OutDefinition)
{
	const UWB_TeamDefinitionData* Data = GetTeamDefinitions();
	if (!Data)
	{
		OutDefinition = FWB_TeamDefinition();
		return false;
	}
	return Data->FindTeamDefinition(TeamIndex, OutDefinition);
}
