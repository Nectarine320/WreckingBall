#include "Player/Command/WB_PlayerCommandComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Ball/Dribble/WB_BallDribbleComponent.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Match/WB_TeamLibrary.h"
#include "Player/Dash/WB_PlayerDashProfile.h"
#include "Player/Pass/WB_PassProfile.h"
#include "Player/Energy/WB_PlayerEnergyProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"
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

void UWB_PlayerCommandComponent::KickPress()
{
	OnKickPressCommand.Broadcast();
}

void UWB_PlayerCommandComponent::KickRelease()
{
	OnKickReleaseCommand.Broadcast();
}

bool UWB_PlayerCommandComponent::IsHoldingBall() const
{
	const AActor* Owner = GetOwner();
	const UWB_PlayerStateComponent* State = Owner ? Owner->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	return State && State->CurrentHolding.MatchesTag(WBTags::Player_Holding_Ball);
}

bool UWB_PlayerCommandComponent::ComputePassVelocity(APawn* Receiver, FVector& OutVelocity, float Charge) const
{
	OutVelocity = FVector::ZeroVector;
	const AActor* Owner = GetOwner();
	if (!Owner || !Receiver || !HasPassProfile())
	{
		return false;
	}
	const UWB_BallDribbleComponent* Dribble = Owner->FindComponentByClass<UWB_BallDribbleComponent>();
	const AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;
	if (!Ball)
	{
		return false;
	}
	float MaxKickSpeed = 0.f;
	if (!PassProfile->ReadBallMaxKickSpeed(Ball, MaxKickSpeed))
	{
		return false;
	}
	return PassProfile->ComputePassVelocity(Ball->GetActorLocation(), Receiver->GetActorLocation(), Receiver->GetVelocity(), MaxKickSpeed, OutVelocity, PassProfile->GetChargeSpeedMultiplier(Charge));
}

bool UWB_PlayerCommandComponent::HasPassProfile() const
{
	if (PassProfile)
	{
		return true;
	}
	if (!bWarnedNoPassProfile)
	{
		bWarnedNoPassProfile = true;
		UE_LOG(LogWBPlayerCommand, Warning, TEXT("%s: no PassProfile on %s (DA_Pass_Default expected). Passes are refused."), *GetNameSafe(GetOwner()), *GetName());
	}
	return false;
}

bool UWB_PlayerCommandComponent::CanPassNow() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !IsHoldingBall())
	{
		return false;
	}
	const UWB_PlayerStateComponent* State = Owner->FindComponentByClass<UWB_PlayerStateComponent>();
	return State && State->IsInState(WBTags::Player_State_Free);
}

bool UWB_PlayerCommandComponent::FirePass(APawn* Receiver, const FVector& FlatVelocity, float Charge)
{
	AActor* Owner = GetOwner();
	const UWB_BallDribbleComponent* Dribble = Owner ? Owner->FindComponentByClass<UWB_BallDribbleComponent>() : nullptr;
	AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;

	bChargingPass = false;
	// Strong charged pass: a small lift (PassMaxLoftAngle). The facing stays flat.
	const FVector Velocity = PassProfile ? PassProfile->ApplyPassLoft(FlatVelocity, Charge) : FlatVelocity;
	SetFacingDirection(FlatVelocity);
	OnPassCommand.Broadcast(Receiver, Velocity);
	const bool bPassed = !IsHoldingBall();
	if (bPassed && Receiver && Ball)
	{
		// The receiver bot reads it to run to the ball's path (ReceivePass).
		UWB_BallPossessionSubsystem::MarkBallPassed(this, Ball, Owner, Receiver);
	}
	return bPassed;
}

bool UWB_PlayerCommandComponent::BeginPassCharge()
{
	if (!CanPassNow() || !HasPassProfile() || !PassProfile->bUseChargedPass)
	{
		return false;
	}
	bChargingPass = true;
	PassChargeStartTime = GetWorld()->GetTimeSeconds();
	return true;
}

void UWB_PlayerCommandComponent::CancelPassCharge()
{
	bChargingPass = false;
}

float UWB_PlayerCommandComponent::GetPassCharge() const
{
	if (!bChargingPass || !PassProfile || !GetWorld())
	{
		return 0.f;
	}
	return PassProfile->GetChargeFromHoldTime(static_cast<float>(GetWorld()->GetTimeSeconds() - PassChargeStartTime));
}

bool UWB_PlayerCommandComponent::RequestPass(APawn* Receiver, float Charge)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Receiver || Receiver == Owner || !CanPassNow())
	{
		return false;
	}
	FVector Velocity;
	if (!ComputePassVelocity(Receiver, Velocity, Charge))
	{
		return false;
	}
	return FirePass(Receiver, Velocity, Charge);
}

bool UWB_PlayerCommandComponent::RequestDirectionalPass(FVector Direction, APawn*& OutReceiver, float Charge)
{
	OutReceiver = nullptr;
	APawn* Owner = Cast<APawn>(GetOwner());
	if (!Owner || !CanPassNow() || !HasPassProfile())
	{
		return false;
	}

	Direction.Z = 0.f;
	if (!PassProfile->bUseAimAssist)
	{
		// Legacy: the teammate best aligned with the stick, whatever the angle.
		APawn* Receiver = UWB_TeamLibrary::FindDirectionalAlly(Owner, Direction, false);
		if (!Receiver || !RequestPass(Receiver, Charge))
		{
			return false;
		}
		OutReceiver = Receiver;
		return true;
	}

	// Stick at rest: the player's facing. Below 0.01 the stick is at rest (technical value, input vectors are 0..1).
	if (!Direction.Normalize(0.01f))
	{
		Direction = Owner->GetActorForwardVector().GetSafeNormal2D();
	}

	// The best aligned teammate gets the pass only inside the cone.
	if (APawn* Candidate = UWB_TeamLibrary::FindDirectionalAlly(Owner, Direction, false))
	{
		const FVector ToCandidate = (Candidate->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(ToCandidate, Direction) >= FMath::Cos(FMath::DegreesToRadians(PassProfile->AimAssistAngle)))
		{
			if (!RequestPass(Candidate, Charge))
			{
				return false;
			}
			OutReceiver = Candidate;
			return true;
		}
	}

	// Free pass along the stick.
	const UWB_BallDribbleComponent* Dribble = Owner->FindComponentByClass<UWB_BallDribbleComponent>();
	const AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;
	float MaxKickSpeed = 0.f;
	FVector Velocity;
	if (!Ball || !PassProfile->ReadBallMaxKickSpeed(Ball, MaxKickSpeed) || !PassProfile->ComputeFreePassVelocity(Direction, MaxKickSpeed, Velocity, PassProfile->GetChargeSpeedMultiplier(Charge)))
	{
		return false;
	}
	return FirePass(nullptr, Velocity, Charge);
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
