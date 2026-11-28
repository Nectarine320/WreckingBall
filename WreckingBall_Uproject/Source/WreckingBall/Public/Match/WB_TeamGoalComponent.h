#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WB_TeamGoalComponent.generated.h"

/**
 * Identifies a goal for gameplay code (bots today): which team defends it, and where to aim.
 * Added to BP_GoalPost_01; its location is the aim point: the middle of the goal line, on the floor (the mouth rises from it, MouthHeight).
 * DefendingTeamIndex is filled by the goal's Construction Script from Is Red Team (0 = red, 1 = blue), so the goal keeps a single source of truth.
 */
UCLASS(Blueprintable, ClassGroup = (WreckingBall), meta = (BlueprintSpawnableComponent))
class WRECKINGBALL_API UWB_TeamGoalComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWB_TeamGoalComponent();

	/** Team that defends this goal (0 = red, 1 = blue). A ball entering it scores for the other team. -1: no team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamGoal")
	int32 DefendingTeamIndex = -1;

	/**
	 * Half width in cm of the goal mouth, along the goal line, centered on the component. Read by the Goalkeeper bot: its shield grid
	 * (ShieldGridColumns x ShieldGridRows rays) covers the mouth, and a shot counts as on target when it crosses it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamGoal", meta = (ClampMin = "1", Units = "Centimeters"))
	float MouthHalfWidth = 280.f;

	/** Height in cm of the goal mouth, measured up from the component (the component sits on the goal line, on the floor). See MouthHalfWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamGoal", meta = (ClampMin = "1", Units = "Centimeters"))
	float MouthHeight = 240.f;

	/** Flat unit vector along the goal mouth (the goal's width axis): the field direction turned by 90 degrees. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal")
	FVector GetMouthAxis() const;

	/** World point a shooter aims at: the component location. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal")
	FVector GetAimLocation() const { return GetComponentLocation(); }

	/** Flat direction from this goal toward the field: toward the average of the other goals, or the component forward when it is the only goal. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal")
	FVector GetFieldDirection() const;

	/** The goal a team must score in: the nearest goal of the world not defended by AttackingTeamIndex. -1 team: the nearest goal. None when the world has no goal. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal", meta = (WorldContext = "WorldContextObject"))
	static UWB_TeamGoalComponent* FindGoalToAttack(const UObject* WorldContextObject, int32 AttackingTeamIndex, FVector FromLocation);

	/** The goal TeamIndex defends (DefendingTeamIndex == TeamIndex). None when the world has none or TeamIndex is -1. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal", meta = (WorldContext = "WorldContextObject"))
	static UWB_TeamGoalComponent* FindGoalDefendedBy(const UObject* WorldContextObject, int32 TeamIndex);

	/** Every registered goal of the world of WorldContextObject. */
	UFUNCTION(BlueprintPure, Category = "TeamGoal", meta = (WorldContext = "WorldContextObject"))
	static TArray<UWB_TeamGoalComponent*> GetAllGoals(const UObject* WorldContextObject);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	/** Goals of every world (filtered by world on query). Cheaper than iterating the actors of a destruction-heavy level. */
	static TArray<TWeakObjectPtr<UWB_TeamGoalComponent>> Registry;
};
