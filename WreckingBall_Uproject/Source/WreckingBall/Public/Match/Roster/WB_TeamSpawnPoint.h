#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WB_TeamSpawnPoint.generated.h"

class UArrowComponent;
class UTextRenderComponent;

/**
 * Where the characters of one team appear, at kickoff and after each goal (devis match 3v3, lot A). Place one per team in the map
 * (BP_TeamSpawnPoint). It holds 3 places (Place1 to Place3, moved in the viewport: put them on the floor, the arrow gives the facing)
 * and a main point (future kickoff ball location, see bUseMainPointForBall). The editor preview (place numbers, arrows) takes the
 * team ring color from the team definitions and is hidden in game.
 */
UCLASS(Blueprintable)
class WRECKINGBALL_API AWB_TeamSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AWB_TeamSpawnPoint();

	/** Team placed here: 0 = red, 1 = blue. Use one spawn point per team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamSpawn", meta = (ClampMin = "0", ClampMax = "1"))
	int32 TeamIndex = 0;

	/** True: after this team concedes a goal, the new ball appears at MainPoint. False: the ball always appears at the world origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TeamSpawn")
	bool bUseMainPointForBall = false;

	/** Main point of the team (kickoff ball location when bUseMainPointForBall is on). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSpawn")
	TObjectPtr<USceneComponent> MainPoint;

	/** Place 1: the first human of the team. On the floor; its forward (arrow) is the facing at kickoff. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSpawn")
	TObjectPtr<USceneComponent> Place1;

	/** Place 2. On the floor; its forward (arrow) is the facing at kickoff. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSpawn")
	TObjectPtr<USceneComponent> Place2;

	/** Place 3. On the floor; its forward (arrow) is the facing at kickoff. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TeamSpawn")
	TObjectPtr<USceneComponent> Place3;

	/** Number of places (3). */
	UFUNCTION(BlueprintPure, Category = "TeamSpawn")
	int32 GetPlaceCount() const { return 3; }

	/** World transform of place PlaceIndex (0 = Place1). Returns false when the index has no place. */
	UFUNCTION(BlueprintPure, Category = "TeamSpawn", meta = (ReturnDisplayName = "Found"))
	bool GetPlaceTransform(int32 PlaceIndex, FTransform& OutTransform) const;

	/** Spawn point of TeamIndex in the world. None when there is none; a warning is logged when there are several (the first one is used). */
	UFUNCTION(BlueprintPure, Category = "TeamSpawn", meta = (WorldContext = "WorldContextObject"))
	static AWB_TeamSpawnPoint* FindForTeam(const UObject* WorldContextObject, int32 InTeamIndex);

	/**
	 * Kickoff ball location (on the floor). ConcedingTeamIndex: team that just conceded a goal, -1 at the match start.
	 * The MainPoint of that team's spawn point when its bUseMainPointForBall is on; else the world origin projected on the NavMesh
	 * (the origin itself, with a warning, when there is no NavMesh there).
	 */
	UFUNCTION(BlueprintPure, Category = "TeamSpawn", meta = (WorldContext = "WorldContextObject"))
	static FVector GetKickoffBallLocation(const UObject* WorldContextObject, int32 ConcedingTeamIndex = -1);

	/**
	 * Puts Actor on the floor below it: traces down (Visibility) from above its collision bounds and moves it so the bottom of its colliding
	 * components rests on the hit floor, whatever its pivot. Call right after spawning a ball. Returns false (Actor not moved) when no floor is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamSpawn", meta = (ReturnDisplayName = "Placed"))
	static bool PlaceOnFloor(AActor* Actor);

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> PreviewLabels;

	UPROPERTY()
	TArray<TObjectPtr<UArrowComponent>> PreviewArrows;

	/** Adds the preview number and arrow under Parent. */
	void AddPreview(USceneComponent* Parent, const FName& BaseName, const FText& Label);
};
