#include "Player/Command/WB_PlayerCommandComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UWB_PlayerCommandComponent::UWB_PlayerCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_PlayerCommandComponent::MoveInWorldDirection(FVector WorldDirection, float Scale)
{
	WorldDirection.Z = 0.f;
	if (!WorldDirection.Normalize() || Scale <= 0.f)
	{
		StopMoving();
		return;
	}

	float X = 0.f;
	float Y = 0.f;
	WorldDirectionToStickAxes(WorldDirection * FMath::Clamp(Scale, 0.f, 1.f), X, Y);
	bMoving = true;
	OnMoveCommand.Broadcast(X, Y);
}

void UWB_PlayerCommandComponent::StopMoving()
{
	if (!bMoving)
	{
		return;
	}
	bMoving = false;
	OnMoveCommand.Broadcast(0.f, 0.f);
}

void UWB_PlayerCommandComponent::Dash()
{
	OnDashCommand.Broadcast();
}

void UWB_PlayerCommandComponent::Grab()
{
	OnGrabCommand.Broadcast();
}

void UWB_PlayerCommandComponent::KickPress()
{
	OnKickPressCommand.Broadcast();
}

void UWB_PlayerCommandComponent::KickRelease()
{
	OnKickReleaseCommand.Broadcast();
}

void UWB_PlayerCommandComponent::SetFacingDirection(FVector WorldDirection)
{
	AActor* Owner = GetOwner();
	WorldDirection.Z = 0.f;
	if (!Owner || !WorldDirection.Normalize())
	{
		return;
	}

	const FRotator Facing(0.f, WorldDirection.Rotation().Yaw, 0.f);
	Owner->SetActorRotation(Facing);

	// The controller rotation follows, so a controller-driven yaw does not turn the pawn back.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(Facing);
		}
	}
}

void UWB_PlayerCommandComponent::WorldDirectionToStickAxes(FVector WorldDirection, float& XAxis, float& YAxis) const
{
	// Same axes as BP_GenericCharacter.Move: right and forward of the shared camera, flattened.
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FRotator Yaw(0.f, Camera->GetCameraRotation().Yaw, 0.f);
		Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
		Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	}

	WorldDirection.Z = 0.f;
	XAxis = FVector::DotProduct(WorldDirection, Right);
	YAxis = FVector::DotProduct(WorldDirection, Forward);
}
