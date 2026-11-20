#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Team/WB_SoccerTeamTypes.h"
#include "WB_TeamCoordinatorComponent.generated.h"

class APlayerController;
class UWB_SoccerAIProfile;
class UWB_TeamGoalComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWB_OnTeamRolesChanged, int32, TeamIndex);

/**
 * Team manager of the phase F (devis NPC): the only one that decides the roles of its team (Attacker, Defender, Goalkeeper).
 * Two instances on BP_SoccerGamemode, one per team (TeamIndex). Omniscient: it reads every member, the opponents, the ball and the goals.
 * Roles: Attacker = the carrier when the team holds the ball, else the member nearest to the ball; Goalkeeper = the nearest to its goal among the others;
 * Defender = the rest. On top of that, one bot always presses the opponent attacker. Hysteresis (AIProfile Team section) against flicker.
 * Humans get no order: their role is deduced from their position, and the bots are organized around them.
 * Each bot (AWB_SoccerAIController_Base) receives its role through SetTeamRole. Active only when AIProfile->bUseTeamRoles.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_TeamCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWB_TeamCoordinatorComponent();

	/** Team managed by this coordinator: 0 = red, 1 = blue (same values as UWB_TeamGoalComponent::DefendingTeamIndex). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamCoordinator")
	int32 TeamIndex = 0;

	/** Bot tuning, Team section (DA_SoccerAIProfile_Default, the same asset as the bots). Required: empty, no role is given and a warning is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamCoordinator")
	TObjectPtr<UWB_SoccerAIProfile> AIProfile;

	/** Called after an evaluation changed a role, the press target or the team situation. */
	UPROPERTY(BlueprintAssignable, Category = "TeamCoordinator")
	FWB_OnTeamRolesChanged OnRolesChanged;

	/** Evaluates the roles now and sends them to the bots. bIgnoreHoldTime: skips RoleMinHoldTime (the distance margin still applies). */
	UFUNCTION(BlueprintCallable, Category = "TeamCoordinator")
	void EvaluateRoles(bool bIgnoreHoldTime = false);

	/** Situation of the team at the last evaluation. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator")
	EWB_TeamSituation GetSituation() const { return Situation; }

	/** Role of Member at the last evaluation. None when Member is not in the team. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator")
	EWB_SoccerRole GetRoleOf(const APawn* Member) const;

	/** First member holding Role at the last evaluation. None when nobody holds it. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator")
	APawn* GetMemberWithRole(EWB_SoccerRole Role) const;

	/** Every member and its role, at the last evaluation. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator")
	TArray<FWB_TeamRoleAssignment> GetAssignments() const { return Assignments; }

	/** Opponent attacker at the last evaluation: the Attacker of the other coordinator, else the opponent carrier, else the opponent nearest to the ball. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator")
	APawn* GetOpponentAttacker() const { return OpponentAttacker.Get(); }

	/** Coordinator of TeamIndex on the current GameMode. None when the GameMode has none. */
	UFUNCTION(BlueprintPure, Category = "TeamCoordinator", meta = (WorldContext = "WorldContextObject"))
	static UWB_TeamCoordinatorComponent* GetTeamCoordinator(const UObject* WorldContextObject, int32 InTeamIndex);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FWB_TeamRoleAssignment> Assignments;

	EWB_TeamSituation Situation = EWB_TeamSituation::Defending;
	TWeakObjectPtr<APawn> OpponentAttacker;
	/** Bot pressing the opponent attacker, and since when (hysteresis). */
	TWeakObjectPtr<APawn> Presser;
	double PresserSince = 0.0;
	TWeakObjectPtr<AActor> CachedBall;
	double NextBallSearchTime = 0.0;
	FTimerHandle EvaluateTimer;
	bool bWarnedNoProfile = false;

	/** Timer tick: EvaluateRoles with the hold time. */
	void HandleTimer();

	UFUNCTION()
	void HandleBallPossessionChanged(AActor* Ball, AActor* NewPossessor, AActor* OldPossessor);

	UFUNCTION()
	void HandleCharacterSwitched(APlayerController* PlayerController, APawn* OldCharacter, APawn* NewCharacter);

	UFUNCTION()
	void HandleRosterCharacterAdded(APawn* Character, int32 InTeamIndex, int32 PlaceIndex);

	/** Players of InTeamIndex: the roster characters, else every pawn of that team with a UWB_PlayerStateComponent. */
	void GatherTeam(int32 InTeamIndex, TArray<APawn*>& OutPawns) const;

	/** The match ball (AIProfile->BallClass), searched again every BallSearchInterval while it is invalid. */
	AActor* GetBall();

	/** Previous assignment of Member, or null. */
	const FWB_TeamRoleAssignment* FindPrevious(const APawn* Member) const;

	/**
	 * Hysteresis pick: the candidate with the smallest Score (cm), unless Previous (the last holder) is still a candidate and is not worse by more than
	 * RoleSwitchMargin, or still within RoleMinHoldTime since PreviousSince (unless bIgnoreHoldTime).
	 */
	APawn* PickWithHysteresis(const TArray<APawn*>& Candidates, APawn* Previous, double PreviousSince, TFunctionRef<double(const APawn*)> Score,
		double Now, bool bIgnoreHoldTime) const;

	/** True while Member cannot play (Down, Stunned, Disabled). */
	static bool IsIncapacitated(const APawn* Member);

	void DrawDebug() const;
};
