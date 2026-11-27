#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "WB_TeamDefinitionData.generated.h"

class UMaterialInterface;

/** Look of one team: name, colors of the overhead label and the ground ring, and the body material. */
USTRUCT(BlueprintType)
struct WRECKINGBALL_API FWB_TeamDefinition
{
	GENERATED_BODY()

	/** Team name shown to the players (results screen, menus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FText DisplayName;

	/** Color of the overhead label text (P1-P4). Its outline stays white. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FLinearColor LabelColor = FLinearColor::White;

	/** Color of the ring decal under the player (RingColor of M_GroundRing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FLinearColor RingColor = FLinearColor::White;

	/** Color of the off-screen indicator sphere of the team's humans (P1-P4 at the screen edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FLinearColor IndicatorColor = FLinearColor::White;

	/** Material put on the character mesh (slot 0): MI_TeamRed, MI_TeamBlue. Empty: the mesh material is not changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	TObjectPtr<UMaterialInterface> TeamMaterial = nullptr;
};

/**
 * Look of every team in one place (DA_TeamDefinitions, UI 3D devis, lot A). Teams[TeamIndex] is the team TeamIndex (0 = red, 1 = blue).
 * The project uses the asset set in Project Settings > Game > Wrecking Ball Teams; read it with UWB_TeamLibrary::GetTeamDefinition.
 */
UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Match"))
class WRECKINGBALL_API UWB_TeamDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** One entry per team, in TeamIndex order: 0 = red, 1 = blue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	TArray<FWB_TeamDefinition> Teams;

	/** Look of a player without a team (TeamIndex -1, or an index with no entry in Teams). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FWB_TeamDefinition NoTeam;

	/** Definition of TeamIndex. Returns false, and NoTeam in OutDefinition, when TeamIndex has no entry in Teams. */
	UFUNCTION(BlueprintPure, Category = "Team", meta = (ReturnDisplayName = "Found"))
	bool FindTeamDefinition(int32 TeamIndex, FWB_TeamDefinition& OutDefinition) const;
};

/** Project setting that points to the team definitions used in game. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wrecking Ball Teams"))
class WRECKINGBALL_API UWB_TeamDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Team definitions (DA_TeamDefinitions) read by UWB_TeamLibrary::GetTeamDefinition. Empty: team colors are not applied and a warning is logged. */
	UPROPERTY(Config, EditAnywhere, Category = "Team")
	TSoftObjectPtr<UWB_TeamDefinitionData> TeamDefinitions;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
