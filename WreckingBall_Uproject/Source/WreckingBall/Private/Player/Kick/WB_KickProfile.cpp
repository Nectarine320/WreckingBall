#include "Player/Kick/WB_KickProfile.h"

#include "Curves/CurveFloat.h"
#include "Data/WB_DataReadLibrary.h"

namespace WBKickPreview
{
	// Variable names of PDA_PhysicsProfile (balls) and PDA_DestructibleProfile (walls), read by reflection for the preview. Technical.
	static const FName MassKg(TEXT("MassKg"));
	static const FName KickImpulse(TEXT("KickImpulse"));
	static const FName MaxKickSpeed(TEXT("MaxKickSpeed"));
	static const FName KickMaxLoftAngle(TEXT("KickMaxLoftAngle"));
	static const FName DamageMultiplier(TEXT("DamageMultiplier"));
	static const FName MinImpactSpeed(TEXT("MinImpactSpeed"));
	static const FName MaxStatesPerHit(TEXT("MaxStatesPerHit"));
	static const FName MaxHealth(TEXT("MaxHealth"));
	static const FName MinDamagePerHit(TEXT("MinDamagePerHit"));
	static const FName EnergyToDamageCurve(TEXT("EnergyToDamageCurve"));
}

float UWB_KickProfile::GetStrengthForCharge(float Charge) const
{
	return FMath::Lerp(KickWeakStrength, KickFullStrength, FMath::Clamp(Charge, 0.f, 1.f));
}

float UWB_KickProfile::GetChargeStep(float TickSeconds) const
{
	return KickChargeTime > UE_KINDA_SMALL_NUMBER ? TickSeconds / KickChargeTime : 1.f;
}

void UWB_KickProfile::PostLoad()
{
	Super::PostLoad();
	RefreshPreview();
}

#if WITH_EDITOR
void UWB_KickProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshPreview();
}
#endif

float UWB_KickProfile::ComputeKickSpeed(float Strength, float& OutUncappedSpeed) const
{
	// ThrowPhysicActor: impulse = strength x KickImpulse, capped at MaxKickSpeed x mass; the ball speed is impulse / mass.
	OutUncappedSpeed = PreviewBallMassKg > 0.f ? Strength * PreviewBallKickImpulse / PreviewBallMassKg : 0.f;
	return PreviewBallMaxKickSpeed > 0.f ? FMath::Min(OutUncappedSpeed, PreviewBallMaxKickSpeed) : OutUncappedSpeed;
}

float UWB_KickProfile::ComputeWallDamage(float Speed, float& OutEnergy) const
{
	// BPFL_Destruction::ComputeImpactDamage: energy = 0.5 x mass x (speed in m/s)^2, then the wall's curve x the ball's DamageMultiplier.
	OutEnergy = 0.5f * PreviewBallMassKg * FMath::Square(Speed / 100.f);
	float DamageMultiplier = 1.f;
	float MinImpactSpeed = 0.f;
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::DamageMultiplier, DamageMultiplier);
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::MinImpactSpeed, MinImpactSpeed);
	const UCurveFloat* Curve = Cast<UCurveFloat>(UWB_DataReadLibrary::ReadObject(SimulatedWall, WBKickPreview::EnergyToDamageCurve));
	if (!Curve || Speed < MinImpactSpeed)
	{
		return 0.f;
	}
	const float Damage = Curve->GetFloatValue(OutEnergy) * DamageMultiplier;
	// The wall refuses a hit under its MinDamagePerHit.
	return Damage >= PreviewWallMinDamage ? Damage : 0.f;
}

int32 UWB_KickProfile::ComputeHits(float Damage) const
{
	if (Damage <= 0.f || PreviewWallHealth <= 0.f)
	{
		return 0;
	}
	float MaxStates = 1.f;
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::MaxStatesPerHit, MaxStates);
	if (MaxStates >= 2.f)
	{
		return FMath::CeilToInt(PreviewWallHealth / Damage);
	}
	// MaxStatesPerHit 1: the first hit on an intact wall leaves at least 1 health.
	return 1 + FMath::CeilToInt(FMath::Max(PreviewWallHealth - Damage, 1.f) / Damage);
}

void UWB_KickProfile::RefreshPreview()
{
	PreviewBallMassKg = PreviewBallKickImpulse = PreviewBallMaxKickSpeed = PreviewMaxLoftAngle = 0.f;
	PreviewWallHealth = PreviewWallMinDamage = 0.f;
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::MassKg, PreviewBallMassKg);
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::KickImpulse, PreviewBallKickImpulse);
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::MaxKickSpeed, PreviewBallMaxKickSpeed);
	UWB_DataReadLibrary::ReadNumber(SimulatedBallPhysics, WBKickPreview::KickMaxLoftAngle, PreviewMaxLoftAngle);
	UWB_DataReadLibrary::ReadNumber(SimulatedWall, WBKickPreview::MaxHealth, PreviewWallHealth);
	UWB_DataReadLibrary::ReadNumber(SimulatedWall, WBKickPreview::MinDamagePerHit, PreviewWallMinDamage);

	float Uncapped = 0.f;
	PreviewWeakKickSpeed = ComputeKickSpeed(KickWeakStrength, Uncapped);
	PreviewFullKickSpeed = ComputeKickSpeed(KickFullStrength, Uncapped);
	bPreviewFullKickCapped = Uncapped > PreviewFullKickSpeed;

	PreviewWeakDamage = ComputeWallDamage(PreviewWeakKickSpeed, PreviewWeakEnergy);
	PreviewFullDamage = ComputeWallDamage(PreviewFullKickSpeed, PreviewFullEnergy);
	PreviewHitsWeak = ComputeHits(PreviewWeakDamage);
	PreviewHitsFull = ComputeHits(PreviewFullDamage);
}
