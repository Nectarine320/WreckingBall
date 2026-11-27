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
#include "UI/GroundRing/WB_GroundRingComponent.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerCommand, Log, All);

UWB_PlayerCommandComponent::UWB_PlayerCommandComponent()
{
	// Ticks only while a pass or a Dash charges (ground ring, cancels).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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
	DashWithCharge(1.f);
}

void UWB_PlayerCommandComponent::DashWithCharge(float Charge)
{
	ActiveDashCharge = FMath::Clamp(Charge, 0.f, 1.f);
	OnDashCommand.Broadcast();
}

bool UWB_PlayerCommandComponent::CanStartDash() const
{
	const UWB_PlayerStateComponent* State = GetOwnerPlayerState();
	return State && State->CanEnterState(WBTags::Player_State_Dashing) && !IsExhausted();
}

void UWB_PlayerCommandComponent::PressDash()
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	if (!Profile || !Profile->bUseChargedDash)
	{
		Dash();
		return;
	}
	if (!CanStartDash())
	{
		return;
	}
	bChargingDash = true;
	DashChargeStartTime = GetWorld()->GetTimeSeconds();
	StartChargeTick();
}

void UWB_PlayerCommandComponent::ReleaseDash()
{
	if (!bChargingDash)
	{
		return;
	}
	const float Charge = GetDashCharge();
	bChargingDash = false;
	SetChargeRing(0.f);
	// RequestDash (Blueprint) consumes the same DashEnergyCost whatever the charge.
	DashWithCharge(Charge);
}

float UWB_PlayerCommandComponent::GetDashCharge() const
{
	const UWB_PlayerDashProfile* Profile = DashProfile.Get();
	if (!bChargingDash || !Profile || !GetWorld())
	{
		return 0.f;
	}
	return Profile->GetChargeFromHoldTime(static_cast<float>(GetWorld()->GetTimeSeconds() - DashChargeStartTime));
}

float UWB_PlayerCommandComponent::GetActiveDashDistance() const
{
	return GetDashDistanceForCharge(ActiveDashCharge);
}

float UWB_PlayerCommandComponent::GetActiveDashDuration() const
{
	return GetDashDurationForCharge(ActiveDashCharge);
}

float UWB_PlayerCommandComponent::GetActiveDashSpeed() const
{
	const float Duration = GetActiveDashDuration();
	return Duration > UE_KINDA_SMALL_NUMBER ? GetActiveDashDistance() / Duration : 0.f;
}

float UWB_PlayerCommandComponent::ResolveDashImpactDamage(bool bIsDashImpact, AActor* Source, FVector ImpactVelocity, float CurveDamage)
{
	const UWB_PlayerCommandComponent* Commands = bIsDashImpact && Source ? Source->FindComponentByClass<UWB_PlayerCommandComponent>() : nullptr;
	const UWB_PlayerDashProfile* Profile = Commands ? Commands->DashProfile.Get() : nullptr;
	if (!Profile || !Profile->bUseChargedDashDamage)
	{
		return CurveDamage;
	}
	// The impact velocity is the Dash speed x the angle factor (ComputeDashImpactVelocity): its ratio gives the angle factor back.
	const float Speed = Commands->GetActiveDashSpeed();
	const float AngleFactor = Speed > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(ImpactVelocity.Size() / Speed, 0.f, 1.f) : 1.f;
	return Profile->GetDamageForCharge(Commands->GetActiveDashCharge()) * AngleFactor;
}

void UWB_PlayerCommandComponent::StartChargeTick()
{
	SetComponentTickEnabled(true);
}

void UWB_PlayerCommandComponent::SetChargeRing(float Percent) const
{
	if (UWB_GroundRingComponent* Ring = GetOwnerGroundRing())
	{
		Ring->SetKickPercent(Percent);
	}
}

void UWB_PlayerCommandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// A hit, a state change or the loss of the ball cancels the charge.
	if (bChargingPass && !CanPassNow())
	{
		bChargingPass = false;
	}
	if (bChargingDash && !CanStartDash())
	{
		bChargingDash = false;
	}
	if (!bChargingPass && !bChargingDash)
	{
		SetChargeRing(0.f);
		SetComponentTickEnabled(false);
		return;
	}
	SetChargeRing(bChargingDash ? GetDashCharge() : GetPassCharge());
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
	const UWB_PlayerStateComponent* State = GetOwnerPlayerState();
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
	const UWB_BallDribbleComponent* Dribble = GetOwnerDribble();
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
	const UWB_PlayerStateComponent* State = GetOwnerPlayerState();
	return State && State->IsInState(WBTags::Player_State_Free);
}

bool UWB_PlayerCommandComponent::FirePass(APawn* Receiver, const FVector& FlatVelocity, float Charge)
{
	AActor* Owner = GetOwner();
	const UWB_BallDribbleComponent* Dribble = GetOwnerDribble();
	AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;

	bChargingPass = false;
	// Strong charged pass: a small lift (PassFullChargeMaxLoftAngle). The facing stays flat.
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
	StartChargeTick();
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
	if (!PassProfile->bUsePassAimAssist)
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
		if (FVector::DotProduct(ToCandidate, Direction) >= FMath::Cos(FMath::DegreesToRadians(PassProfile->PassAimAssistAngle)))
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
	const UWB_BallDribbleComponent* Dribble = GetOwnerDribble();
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
	const UWB_PlayerStateComponent* State = GetOwnerPlayerState();
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

float UWB_PlayerCommandComponent::GetDashDistanceForCharge(float Charge) const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->GetDistanceForCharge(Charge) : 0.f;
}

float UWB_PlayerCommandComponent::GetDashDurationForCharge(float Charge) const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->GetDurationForCharge(Charge) : 0.f;
}

float UWB_PlayerCommandComponent::GetDashMinRunUp() const
{
	const UWB_PlayerDashProfile* Profile = GetDashProfile();
	return Profile ? Profile->DashMinRunUpDistancePlayer : 0.f;
}

UWB_PlayerStateComponent* UWB_PlayerCommandComponent::GetOwnerPlayerState() const
{
	if (!CachedPlayerState.IsValid())
	{
		const AActor* Owner = GetOwner();
		CachedPlayerState = Owner ? Owner->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	}
	return CachedPlayerState.Get();
}

UWB_BallDribbleComponent* UWB_PlayerCommandComponent::GetOwnerDribble() const
{
	if (!CachedDribble.IsValid())
	{
		const AActor* Owner = GetOwner();
		CachedDribble = Owner ? Owner->FindComponentByClass<UWB_BallDribbleComponent>() : nullptr;
	}
	return CachedDribble.Get();
}

UWB_GroundRingComponent* UWB_PlayerCommandComponent::GetOwnerGroundRing() const
{
	if (!CachedGroundRing.IsValid())
	{
		const AActor* Owner = GetOwner();
		CachedGroundRing = Owner ? Owner->FindComponentByClass<UWB_GroundRingComponent>() : nullptr;
	}
	return CachedGroundRing.Get();
}
