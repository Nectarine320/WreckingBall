#include "Player/Command/WB_PlayerCommandComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Dash/WB_PlayerDashProfile.h"
#include "Player/Energy/WB_PlayerEnergyProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerCommand, Log, All);

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

void UWB_PlayerCommandComponent::Tap()
{
	OnTapCommand.Broadcast();
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

bool UWB_PlayerCommandComponent::ReadOwnerNumber(FName Variable, float& OutValue) const
{
	const AActor* Owner = GetOwner();
	const FNumericProperty* Property = Owner ? FindFProperty<FNumericProperty>(Owner->GetClass(), Variable) : nullptr;
	if (!Property)
	{
		if (Owner && !WarnedVariables.Contains(Variable))
		{
			WarnedVariables.Add(Variable);
			UE_LOG(LogWBPlayerCommand, Warning, TEXT("%s: no number variable '%s' on %s (readout of UWB_PlayerCommandComponent)."), *GetName(), *Variable.ToString(), *Owner->GetClass()->GetName());
		}
		return false;
	}
	const void* Value = Property->ContainerPtrToValuePtr<void>(Owner);
	OutValue = Property->IsFloatingPoint()
		? static_cast<float>(Property->GetFloatingPointPropertyValue(Value))
		: static_cast<float>(Property->GetSignedIntPropertyValue(Value));
	return true;
}

float UWB_PlayerCommandComponent::GetEnergyPercent() const
{
	float Value = 1.f;
	return ReadOwnerNumber(EnergyPercentVariable, Value) ? FMath::Clamp(Value, 0.f, 1.f) : 1.f;
}

bool UWB_PlayerCommandComponent::IsExhausted() const
{
	const AActor* Owner = GetOwner();
	const FProperty* Property = Owner ? FindFProperty<FProperty>(Owner->GetClass(), WalkTypeVariable) : nullptr;
	if (!Property)
	{
		return false;
	}

	// A Blueprint enum variable is a byte property with an enum; a C++ enum class is an enum property.
	const UEnum* Enum = nullptr;
	int64 Value = 0;
	const void* Data = Property->ContainerPtrToValuePtr<void>(Owner);
	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		Enum = ByteProperty->Enum;
		Value = ByteProperty->GetPropertyValue(Data);
	}
	else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProperty->GetEnum();
		Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Data);
	}
	return Enum && Enum->GetDisplayNameTextByValue(Value).ToString() == ExhaustedWalkType;
}

float UWB_PlayerCommandComponent::GetDashEnergyCost() const
{
	const AActor* Owner = GetOwner();
	const UWB_PlayerStateComponent* State = Owner ? Owner->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	const UWB_PlayerEnergyProfile* Energy = State ? State->GetActiveEnergyProfile() : nullptr;
	return Energy ? Energy->DashEnergyCost : 1.f;
}

const UWB_PlayerDashProfile* UWB_PlayerCommandComponent::GetDashProfile() const
{
	if (!DashProfile && !bWarnedNoDashProfile)
	{
		bWarnedNoDashProfile = true;
		UE_LOG(LogWBPlayerCommand, Warning, TEXT("%s: no DashProfile on %s (DA_PlayerDash_Default expected). The Dash settings read None, the bots never Dash."), *GetNameSafe(GetOwner()), *GetName());
	}
	return DashProfile.Get();
}

float UWB_PlayerCommandComponent::GetDashDistance() const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->DashDistance : 0.f;
}

float UWB_PlayerCommandComponent::GetDashDuration() const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->DashDuration : 0.f;
}

float UWB_PlayerCommandComponent::GetDashMinRunUp() const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->DashMinRunUpDistancePlayer : 0.f;
}
