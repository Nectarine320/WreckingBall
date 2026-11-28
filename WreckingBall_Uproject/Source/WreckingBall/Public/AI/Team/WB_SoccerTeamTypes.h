#pragma once

#include "CoreMinimal.h"
#include "WB_SoccerTeamTypes.generated.h"

/** Role a team coordinator (UWB_TeamCoordinatorComponent) gives to a member of its team. Fixed list of the team brain. */
UENUM(BlueprintType)
enum class EWB_SoccerRole : uint8
{
	/** No role: the bot plays with the legacy brain (no coordinator, or bUseTeamRoles off). */
	None,
	/** Nearest to the ball (or the carrier): goes for the ball, presses the opponent carrier, attacks with the ball. */
	Attacker,
	/** Stays in the defensive zone between the ball and its goal; supports the carrier when the team has the ball. */
	Defender,
	/** Nearest to its goal (team of 2 or more): behavior AI.Behavior.Goalkeeping (guards the goal against threats, builds a shield of objects). */
	Goalkeeper
};

/** What a team is doing, from the ball possession. */
UENUM(BlueprintType)
enum class EWB_TeamSituation : uint8
{
	/** Nobody of the team holds the ball (free ball, or an opponent holds it). */
	Defending,
	/** A member of the team holds the ball. */
	Attacking
};

/** Role of one team member, as decided by the coordinator. */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_TeamRoleAssignment
{
	GENERATED_BODY()

	/** The team member. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TeamCoordinator")
	TObjectPtr<APawn> Member = nullptr;

	/** Its role. A human gets a role too (deduced from its position), but no order. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TeamCoordinator")
	EWB_SoccerRole Role = EWB_SoccerRole::None;

	/** True for the one bot that presses the opponent attacker (PressTarget). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TeamCoordinator")
	bool bPressing = false;

	/** Opponent pressed by this member (bPressing). None otherwise. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "TeamCoordinator")
	TObjectPtr<APawn> PressTarget = nullptr;

	/** World time of the last role change of this member (hysteresis). */
	double RoleSinceTime = 0.0;
};
