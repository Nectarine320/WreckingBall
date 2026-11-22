#include "AI/Team/WB_TeamCoordinatorComponent.h"

#include "Debug/WB_DebugTextSubsystem.h"
#include "AI/WB_SoccerAIController_Base.h"
#include "AI/WB_SoccerAIProfile.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Match/Roster/WB_TeamRosterComponent.h"
#include "Match/WB_TeamGoalComponent.h"
#include "Match/WB_TeamLibrary.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"
#include "TimerManager.h"

namespace
{
	AWB_SoccerAIController_Base* GetBot(const APawn* Pawn)
	{
		return Pawn ? Cast<AWB_SoccerAIController_Base>(Pawn->GetController()) : nullptr;
	}

	double Dist2D(const AActor* A, const FVector& B)
	{
		return A ? FVector::Dist2D(A->GetActorLocation(), B) : TNumericLimits<double>::Max();
	}

	const TCHAR* RoleLabel(EWB_SoccerRole Role)
	{
		switch (Role)
		{
		case EWB_SoccerRole::Attacker:   return TEXT("ATT");
		case EWB_SoccerRole::Defender:   return TEXT("DEF");
		case EWB_SoccerRole::Goalkeeper: return TEXT("GK");
		default:                         return TEXT("-");
		}
	}
}

UWB_TeamCoordinatorComponent::UWB_TeamCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWB_TeamCoordinatorComponent* UWB_TeamCoordinatorComponent::GetTeamCoordinator(const UObject* WorldContextObject, int32 InTeamIndex)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!GameMode)
	{
		return nullptr;
	}
	TArray<UWB_TeamCoordinatorComponent*> Coordinators;
	GameMode->GetComponents(Coordinators);
	for (UWB_TeamCoordinatorComponent* Coordinator : Coordinators)
	{
		if (Coordinator->TeamIndex == InTeamIndex)
		{
			return Coordinator;
		}
	}
	return nullptr;
}

void UWB_TeamCoordinatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!AIProfile)
	{
		bWarnedNoProfile = true;
		UE_LOG(LogTemp, Warning, TEXT("%s (team %d): no AIProfile (DA_SoccerAIProfile_Default expected). No role is given."), *GetName(), TeamIndex);
		return;
	}

	if (UWB_BallPossessionSubsystem* Possession = GetWorld()->GetSubsystem<UWB_BallPossessionSubsystem>())
	{
		Possession->OnBallPossessionChanged.AddDynamic(this, &UWB_TeamCoordinatorComponent::HandleBallPossessionChanged);
	}
	if (UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this))
	{
		Roster->OnCharacterSwitched.AddDynamic(this, &UWB_TeamCoordinatorComponent::HandleCharacterSwitched);
		Roster->OnRosterCharacterAdded.AddDynamic(this, &UWB_TeamCoordinatorComponent::HandleRosterCharacterAdded);
	}
	GetWorld()->GetTimerManager().SetTimer(EvaluateTimer, this, &UWB_TeamCoordinatorComponent::HandleTimer, AIProfile->RoleUpdateInterval, true);
}

void UWB_TeamCoordinatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluateTimer);
		if (UWB_BallPossessionSubsystem* Possession = World->GetSubsystem<UWB_BallPossessionSubsystem>())
		{
			Possession->OnBallPossessionChanged.RemoveDynamic(this, &UWB_TeamCoordinatorComponent::HandleBallPossessionChanged);
		}
	}
	if (UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this))
	{
		Roster->OnCharacterSwitched.RemoveDynamic(this, &UWB_TeamCoordinatorComponent::HandleCharacterSwitched);
		Roster->OnRosterCharacterAdded.RemoveDynamic(this, &UWB_TeamCoordinatorComponent::HandleRosterCharacterAdded);
	}
	Super::EndPlay(EndPlayReason);
}

void UWB_TeamCoordinatorComponent::HandleTimer()
{
	EvaluateRoles(false);
}

void UWB_TeamCoordinatorComponent::HandleBallPossessionChanged(AActor* Ball, AActor* NewPossessor, AActor* OldPossessor)
{
	CachedBall = Ball;
	EvaluateRoles(true);
}

void UWB_TeamCoordinatorComponent::HandleCharacterSwitched(APlayerController* PlayerController, APawn* OldCharacter, APawn* NewCharacter)
{
	EvaluateRoles(true);
}

void UWB_TeamCoordinatorComponent::HandleRosterCharacterAdded(APawn* Character, int32 InTeamIndex, int32 PlaceIndex)
{
	if (InTeamIndex == TeamIndex)
	{
		EvaluateRoles(true);
	}
}

void UWB_TeamCoordinatorComponent::GatherTeam(int32 InTeamIndex, TArray<APawn*>& OutPawns) const
{
	OutPawns.Reset();
	const UWB_TeamRosterComponent* Roster = UWB_TeamRosterComponent::GetTeamRoster(this);
	if (Roster && Roster->IsRosterBuilt())
	{
		for (APawn* Pawn : Roster->GetRosterCharacters())
		{
			if (Pawn && UWB_TeamLibrary::GetTeamOf(Pawn) == InTeamIndex)
			{
				OutPawns.Add(Pawn);
			}
		}
		return;
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->FindComponentByClass<UWB_PlayerStateComponent>() && UWB_TeamLibrary::GetTeamOf(*It) == InTeamIndex)
		{
			OutPawns.Add(*It);
		}
	}
}

AActor* UWB_TeamCoordinatorComponent::GetBall()
{
	if (CachedBall.IsValid())
	{
		return CachedBall.Get();
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextBallSearchTime)
	{
		return nullptr;
	}
	NextBallSearchTime = Now + AIProfile->BallSearchInterval;
	if (UClass* BallClass = AIProfile->BallClass.LoadSynchronous())
	{
		for (TActorIterator<AActor> It(GetWorld(), BallClass); It; ++It)
		{
			CachedBall = *It;
			break;
		}
	}
	return CachedBall.Get();
}

const FWB_TeamRoleAssignment* UWB_TeamCoordinatorComponent::FindPrevious(const APawn* Member) const
{
	return Assignments.FindByPredicate([Member](const FWB_TeamRoleAssignment& Entry) { return Entry.Member == Member; });
}

bool UWB_TeamCoordinatorComponent::IsIncapacitated(const APawn* Member)
{
	const UWB_PlayerStateComponent* State = Member ? Member->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	return State && (State->IsInState(WBTags::Player_State_Down) || State->IsInState(WBTags::Player_State_Stunned)
		|| State->IsInState(WBTags::Player_State_Disabled));
}

APawn* UWB_TeamCoordinatorComponent::PickWithHysteresis(const TArray<APawn*>& Candidates, APawn* Previous, double PreviousSince,
	TFunctionRef<double(const APawn*)> Score, double Now, bool bIgnoreHoldTime) const
{
	APawn* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	for (APawn* Candidate : Candidates)
	{
		const double CandidateScore = Score(Candidate);
		if (CandidateScore < BestScore)
		{
			BestScore = CandidateScore;
			Best = Candidate;
		}
	}
	if (Previous && Previous != Best && Candidates.Contains(Previous))
	{
		if (!bIgnoreHoldTime && Now - PreviousSince < AIProfile->RoleMinHoldTime)
		{
			return Previous;
		}
		if (Score(Previous) <= BestScore + AIProfile->RoleSwitchMargin)
		{
			return Previous;
		}
	}
	return Best;
}

EWB_SoccerRole UWB_TeamCoordinatorComponent::GetRoleOf(const APawn* Member) const
{
	const FWB_TeamRoleAssignment* Entry = FindPrevious(Member);
	return Entry ? Entry->Role : EWB_SoccerRole::None;
}

APawn* UWB_TeamCoordinatorComponent::GetMemberWithRole(EWB_SoccerRole Role) const
{
	const FWB_TeamRoleAssignment* Entry = Assignments.FindByPredicate([Role](const FWB_TeamRoleAssignment& Item) { return Item.Role == Role && Item.Member; });
	return Entry ? Entry->Member.Get() : nullptr;
}

void UWB_TeamCoordinatorComponent::EvaluateRoles(bool bIgnoreHoldTime)
{
	if (!AIProfile)
	{
		if (!bWarnedNoProfile)
		{
			bWarnedNoProfile = true;
			UE_LOG(LogTemp, Warning, TEXT("%s (team %d): no AIProfile (DA_SoccerAIProfile_Default expected). No role is given."), *GetName(), TeamIndex);
		}
		return;
	}
	if (!AIProfile->bUseTeamRoles || TeamIndex < 0)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	TArray<APawn*> Members;
	GatherTeam(TeamIndex, Members);
	TArray<APawn*> Opponents;
	GatherTeam(1 - TeamIndex, Opponents);

	// ---- Situation ----
	AActor* Ball = GetBall();
	APawn* Carrier = Ball ? Cast<APawn>(UWB_BallPossessionSubsystem::GetBallPossessor(this, Ball)) : nullptr;
	const bool bTeamHasBall = Carrier && Members.Contains(Carrier);
	const EWB_TeamSituation NewSituation = bTeamHasBall ? EWB_TeamSituation::Attacking : EWB_TeamSituation::Defending;
	const UWB_TeamGoalComponent* OwnGoal = UWB_TeamGoalComponent::FindGoalDefendedBy(this, TeamIndex);

	// Members able to play first; everyone when nobody can.
	TArray<APawn*> Able = Members.FilterByPredicate([](const APawn* Member) { return !IsIncapacitated(Member); });
	if (Able.IsEmpty())
	{
		Able = Members;
	}

	auto PreviousWithRole = [this](EWB_SoccerRole Role, double& OutSince) -> APawn*
	{
		for (const FWB_TeamRoleAssignment& Entry : Assignments)
		{
			if (Entry.Role == Role && Entry.Member)
			{
				OutSince = Entry.RoleSinceTime;
				return Entry.Member.Get();
			}
		}
		return nullptr;
	};

	// ---- Attacker: the carrier, else the nearest to the ball ----
	APawn* Attacker = nullptr;
	double AttackerSince = 0.0;
	APawn* PreviousAttacker = PreviousWithRole(EWB_SoccerRole::Attacker, AttackerSince);
	if (bTeamHasBall)
	{
		Attacker = Carrier;
	}
	else if (Ball)
	{
		const FVector BallLocation = Ball->GetActorLocation();
		Attacker = PickWithHysteresis(Able, PreviousAttacker, AttackerSince, [&BallLocation](const APawn* P) { return Dist2D(P, BallLocation); }, Now, bIgnoreHoldTime);
	}
	else
	{
		Attacker = Members.Contains(PreviousAttacker) ? PreviousAttacker : (Able.IsEmpty() ? nullptr : Able[0]);
	}

	// ---- Goalkeeper: the nearest to its goal among the others ----
	APawn* Goalkeeper = nullptr;
	if (OwnGoal)
	{
		TArray<APawn*> Candidates = Able.FilterByPredicate([Attacker](const APawn* P) { return P != Attacker; });
		if (Candidates.IsEmpty())
		{
			Candidates = Members.FilterByPredicate([Attacker](const APawn* P) { return P != Attacker; });
		}
		double GoalkeeperSince = 0.0;
		APawn* PreviousGoalkeeper = PreviousWithRole(EWB_SoccerRole::Goalkeeper, GoalkeeperSince);
		const FVector GoalLocation = OwnGoal->GetAimLocation();
		Goalkeeper = PickWithHysteresis(Candidates, PreviousGoalkeeper, GoalkeeperSince, [&GoalLocation](const APawn* P) { return Dist2D(P, GoalLocation); }, Now, bIgnoreHoldTime);
	}

	// ---- Opponent attacker: their carrier, else the Attacker of their coordinator, else the opponent nearest to the ball ----
	APawn* NewOpponentAttacker = (Carrier && Opponents.Contains(Carrier)) ? Carrier : nullptr;
	if (!NewOpponentAttacker)
	{
		if (const UWB_TeamCoordinatorComponent* Other = GetTeamCoordinator(this, 1 - TeamIndex))
		{
			APawn* OtherAttacker = Other->GetMemberWithRole(EWB_SoccerRole::Attacker);
			NewOpponentAttacker = Opponents.Contains(OtherAttacker) ? OtherAttacker : nullptr;
		}
	}
	if (!NewOpponentAttacker && Ball)
	{
		double BestDist = TNumericLimits<double>::Max();
		for (APawn* Opponent : Opponents)
		{
			const double Dist = Dist2D(Opponent, Ball->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				NewOpponentAttacker = Opponent;
			}
		}
	}

	// ---- Build the roles ----
	TArray<FWB_TeamRoleAssignment> NewAssignments;
	for (APawn* Member : Members)
	{
		FWB_TeamRoleAssignment& Entry = NewAssignments.AddDefaulted_GetRef();
		Entry.Member = Member;
		Entry.Role = Member == Attacker ? EWB_SoccerRole::Attacker : (Member == Goalkeeper ? EWB_SoccerRole::Goalkeeper : EWB_SoccerRole::Defender);
		const FWB_TeamRoleAssignment* Previous = FindPrevious(Member);
		Entry.RoleSinceTime = (Previous && Previous->Role == Entry.Role) ? Previous->RoleSinceTime : Now;
	}

	// ---- Press rule: one bot always presses the opponent attacker ----
	APawn* NewPresser = nullptr;
	if (NewOpponentAttacker && (NewSituation == EWB_TeamSituation::Defending || AIProfile->bPressWhileAttacking))
	{
		if (NewSituation == EWB_TeamSituation::Defending && GetBot(Attacker))
		{
			// A bot Attacker already goes for the ball or the carrier.
			NewPresser = Attacker;
		}
		else
		{
			// A human Attacker (or the team holds the ball): a Defender bot presses, a Goalkeeper bot only when no Defender bot can.
			TArray<APawn*> Candidates;
			for (const FWB_TeamRoleAssignment& Entry : NewAssignments)
			{
				if (Entry.Role == EWB_SoccerRole::Defender && Entry.Member != Carrier && GetBot(Entry.Member) && !IsIncapacitated(Entry.Member))
				{
					Candidates.Add(Entry.Member);
				}
			}
			if (Candidates.IsEmpty() && Goalkeeper != Carrier && GetBot(Goalkeeper) && !IsIncapacitated(Goalkeeper))
			{
				Candidates.Add(Goalkeeper);
			}
			const FVector TargetLocation = NewOpponentAttacker->GetActorLocation();
			NewPresser = PickWithHysteresis(Candidates, Presser.Get(), PresserSince, [&TargetLocation](const APawn* P) { return Dist2D(P, TargetLocation); }, Now, bIgnoreHoldTime);
		}
	}
	if (NewPresser != Presser.Get())
	{
		PresserSince = Now;
	}
	Presser = NewPresser;
	for (FWB_TeamRoleAssignment& Entry : NewAssignments)
	{
		Entry.bPressing = Entry.Member == NewPresser;
		Entry.PressTarget = Entry.bPressing ? NewOpponentAttacker : nullptr;
	}

	// ---- Change detection, then send the roles to the bots ----
	bool bChanged = NewSituation != Situation || NewAssignments.Num() != Assignments.Num();
	for (const FWB_TeamRoleAssignment& Entry : NewAssignments)
	{
		const FWB_TeamRoleAssignment* Previous = FindPrevious(Entry.Member);
		bChanged |= !Previous || Previous->Role != Entry.Role || Previous->bPressing != Entry.bPressing || Previous->PressTarget != Entry.PressTarget;
	}

	Situation = NewSituation;
	OpponentAttacker = NewOpponentAttacker;
	Assignments = MoveTemp(NewAssignments);

	for (const FWB_TeamRoleAssignment& Entry : Assignments)
	{
		if (AWB_SoccerAIController_Base* Bot = GetBot(Entry.Member))
		{
			Bot->SetTeamRole(Entry.Role, Situation, Entry.PressTarget);
		}
	}

	if (bChanged)
	{
		OnRolesChanged.Broadcast(TeamIndex);
	}
	DrawDebug();
}

void UWB_TeamCoordinatorComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (!AIProfile || !AIProfile->bDrawRoleDebug)
	{
		return;
	}
	FWB_TeamDefinition Definition;
	UWB_TeamLibrary::GetTeamDefinition(TeamIndex, Definition);
	const FColor Color = Definition.LabelColor.ToFColor(true);
	const float Duration = AIProfile->RoleUpdateInterval * 1.05f;
	for (const FWB_TeamRoleAssignment& Entry : Assignments)
	{
		if (!Entry.Member)
		{
			continue;
		}
		FString Text = RoleLabel(Entry.Role);
		if (Entry.bPressing)
		{
			Text += TEXT(" PRESS");
		}
		if (!GetBot(Entry.Member))
		{
			Text += TEXT(" (H)");
		}
		UWB_DebugTextSubsystem::DrawDebugText(this, FVector(0.f, 0.f, 170.f), Text, Entry.Member, Color, Duration, true, 1.2f);
		if (Entry.PressTarget)
		{
			DrawDebugLine(GetWorld(), Entry.Member->GetActorLocation(), Entry.PressTarget->GetActorLocation(), Color, false, Duration, 0, 2.f);
		}
	}
	if (Assignments.Num() > 0)
	{
		const FString Header = FString::Printf(TEXT("Team %d: %s"), TeamIndex, Situation == EWB_TeamSituation::Attacking ? TEXT("Attacking") : TEXT("Defending"));
		if (const UWB_TeamGoalComponent* OwnGoal = UWB_TeamGoalComponent::FindGoalDefendedBy(this, TeamIndex))
		{
			UWB_DebugTextSubsystem::DrawDebugText(this, OwnGoal->GetAimLocation() + FVector(0.f, 0.f, 250.f), Header, nullptr, Color, Duration, true, 1.2f);
		}
	}
#endif
}
