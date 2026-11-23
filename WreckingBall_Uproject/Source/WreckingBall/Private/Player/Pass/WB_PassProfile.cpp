#include "Player/Pass/WB_PassProfile.h"

#include "Data/WB_DataReadLibrary.h"
#include "Player/Charge/WB_ChargeLibrary.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPass, Log, All);

namespace
{
	/** Iterations of the lead estimate (travel time <-> aim point). Technical value. */
	constexpr int32 LeadIterations = 3;
}

float UWB_PassProfile::GetPassSpeed(float Distance, float MaxKickSpeed, float SpeedMultiplier) const
{
	// Speed that still leaves PassArrivalSpeed at the receiver after rolling Distance: v0^2 = v1^2 + 2 a d. A charged pass multiplies it.
	float Speed = FMath::Sqrt(FMath::Square(PassArrivalSpeed) + 2.f * PassRollingDeceleration * FMath::Max(Distance, 0.f)) * FMath::Max(SpeedMultiplier, 1.f);
	if (MaxKickSpeed > 0.f)
	{
		Speed = FMath::Clamp(Speed, PassMinShareOfMaxKickSpeed * MaxKickSpeed, MaxKickSpeed);
	}
	return Speed;
}

bool UWB_PassProfile::ComputePassVelocity(FVector BallLocation, FVector ReceiverLocation, FVector ReceiverVelocity, float MaxKickSpeed, FVector& OutVelocity, float SpeedMultiplier) const
{
	BallLocation.Z = 0.f;
	ReceiverLocation.Z = 0.f;
	ReceiverVelocity.Z = 0.f;

	FVector AimPoint = ReceiverLocation;
	float Speed = GetPassSpeed(FVector::Dist(BallLocation, AimPoint), MaxKickSpeed, SpeedMultiplier);

	if (PassAimMode == EWB_PassAimMode::Lead)
	{
		for (int32 i = 0; i < LeadIterations; ++i)
		{
			const float Distance = FVector::Dist(BallLocation, AimPoint);
			// Mean speed of a ball slowing down from Speed to (about) PassArrivalSpeed.
			const float MeanSpeed = FMath::Max((Speed + FMath::Min(PassArrivalSpeed, Speed)) * 0.5f, 1.f);
			const float TravelTime = FMath::Min(Distance / MeanSpeed, PassMaxLeadTime);
			AimPoint = ReceiverLocation + ReceiverVelocity * TravelTime;
			Speed = GetPassSpeed(FVector::Dist(BallLocation, AimPoint), MaxKickSpeed, SpeedMultiplier);
		}
	}

	FVector Direction = AimPoint - BallLocation;
	if (!Direction.Normalize())
	{
		OutVelocity = FVector::ZeroVector;
		return false;
	}
	OutVelocity = Direction * Speed;
	return true;
}

bool UWB_PassProfile::ComputeFreePassVelocity(FVector Direction, float MaxKickSpeed, FVector& OutVelocity, float SpeedMultiplier) const
{
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		OutVelocity = FVector::ZeroVector;
		return false;
	}
	OutVelocity = Direction * GetPassSpeed(FreePassDistance, MaxKickSpeed, SpeedMultiplier);
	return true;
}

float UWB_PassProfile::GetChargeFromHoldTime(float HoldTime) const
{
	return bUseChargedPass ? UWB_ChargeLibrary::GetChargeFromHoldTime(HoldTime, PassChargeMinHoldTime, PassChargeTime) : 0.f;
}

float UWB_PassProfile::GetChargeSpeedMultiplier(float Charge) const
{
	return FMath::Lerp(1.f, PassFullChargeSpeedMultiplier, FMath::Clamp(Charge, 0.f, 1.f));
}

FVector UWB_PassProfile::ApplyPassLoft(FVector FlatVelocity, float Charge) const
{
	if (Charge < PassLoftMinCharge || Charge <= 0.f || PassFullChargeMaxLoftAngle <= 0.f)
	{
		return FlatVelocity;
	}
	// Same lift as the kick (ApplyKickLoft): the speed is kept, part of it goes up.
	const float Angle = FMath::DegreesToRadians(FMath::FRandRange(0.f, PassFullChargeMaxLoftAngle));
	const FVector Flat(FlatVelocity.X, FlatVelocity.Y, 0.f);
	return Flat * FMath::Cos(Angle) + FVector(0.f, 0.f, Flat.Size() * FMath::Sin(Angle));
}

bool UWB_PassProfile::ReadBallMaxKickSpeed(const AActor* Ball, float& OutMaxKickSpeed) const
{
	OutMaxKickSpeed = 0.f;
	UFunction* Function = Ball ? Ball->FindFunction(PhysicsProfileFunction) : nullptr;
	if (!Function)
	{
		UE_LOG(LogWBPass, Warning, TEXT("%s: %s has no function '%s' (PhysicsProfileFunction). Pass refused."), *GetName(), *GetNameSafe(Ball), *PhysicsProfileFunction.ToString());
		return false;
	}

	// Calls the Blueprint function and reads its first object output (the profile).
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(Function->ParmsSize, 1)));
	FMemory::Memzero(Params, Function->ParmsSize);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}
	const_cast<AActor*>(Ball)->ProcessEvent(Function, Params);

	UObject* Profile = nullptr;
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It))
		{
			if (!Profile && ObjectProperty->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				Profile = ObjectProperty->GetObjectPropertyValue_InContainer(Params);
			}
		}
	}
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}

	if (!Profile)
	{
		UE_LOG(LogWBPass, Warning, TEXT("%s: %s returned no physics profile. Pass refused."), *GetName(), *GetNameSafe(Ball));
		return false;
	}
	const FNumericProperty* Property = FindFProperty<FNumericProperty>(Profile->GetClass(), MaxKickSpeedVariable);
	if (!Property)
	{
		UE_LOG(LogWBPass, Warning, TEXT("%s: no number variable '%s' on %s (MaxKickSpeedVariable). Pass refused."), *GetName(), *MaxKickSpeedVariable.ToString(), *Profile->GetName());
		return false;
	}
	const void* Value = Property->ContainerPtrToValuePtr<void>(Profile);
	OutMaxKickSpeed = Property->IsFloatingPoint()
		? static_cast<float>(Property->GetFloatingPointPropertyValue(Value))
		: static_cast<float>(Property->GetSignedIntPropertyValue(Value));
	return true;
}

void UWB_PassProfile::PostLoad()
{
	Super::PostLoad();
	RefreshPreview();
}

#if WITH_EDITOR
void UWB_PassProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshPreview();
}
#endif

void UWB_PassProfile::ComputeTravel(float Speed, float Distance, float& OutTime, float& OutArrivalSpeed) const
{
	OutTime = 0.f;
	OutArrivalSpeed = 0.f;
	if (Speed <= 0.f)
	{
		return;
	}
	if (PassRollingDeceleration <= 0.f)
	{
		OutTime = Distance / Speed;
		OutArrivalSpeed = Speed;
		return;
	}
	// v1^2 = v0^2 - 2 a d; negative: the ball stops before the receiver.
	const float ArrivalSquared = FMath::Square(Speed) - 2.f * PassRollingDeceleration * Distance;
	if (ArrivalSquared < 0.f)
	{
		return;
	}
	OutArrivalSpeed = FMath::Sqrt(ArrivalSquared);
	OutTime = (Speed - OutArrivalSpeed) / PassRollingDeceleration;
}

void UWB_PassProfile::RefreshPreview()
{
	PreviewBallMaxKickSpeed = 0.f;
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, MaxKickSpeedVariable, PreviewBallMaxKickSpeed);

	const float FullMultiplier = bUseChargedPass ? PassFullChargeSpeedMultiplier : 1.f;
	PreviewWeakPassSpeed = GetPassSpeed(SimulatedPassDistance, PreviewBallMaxKickSpeed, 1.f);
	PreviewFullPassSpeed = GetPassSpeed(SimulatedPassDistance, PreviewBallMaxKickSpeed, FullMultiplier);
	bPreviewFullPassCapped = PreviewBallMaxKickSpeed > 0.f && GetPassSpeed(SimulatedPassDistance, 0.f, FullMultiplier) > PreviewBallMaxKickSpeed;
	PreviewWeakFreePassSpeed = GetPassSpeed(FreePassDistance, PreviewBallMaxKickSpeed, 1.f);
	PreviewFullFreePassSpeed = GetPassSpeed(FreePassDistance, PreviewBallMaxKickSpeed, FullMultiplier);

	ComputeTravel(PreviewWeakPassSpeed, SimulatedPassDistance, PreviewWeakTravelTime, PreviewWeakArrivalSpeed);
	ComputeTravel(PreviewFullPassSpeed, SimulatedPassDistance, PreviewFullTravelTime, PreviewFullArrivalSpeed);

	PreviewLoftStartHoldTime = bUseChargedPass ? FMath::Lerp(PassChargeMinHoldTime, PassChargeTime, PassLoftMinCharge) : 0.f;
}
