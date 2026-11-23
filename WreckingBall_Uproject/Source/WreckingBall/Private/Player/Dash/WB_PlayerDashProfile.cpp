#include "Player/Dash/WB_PlayerDashProfile.h"

#include "Player/Charge/WB_ChargeLibrary.h"

float UWB_PlayerDashProfile::GetChargeFromHoldTime(float HoldTime) const
{
	return bUseChargedDash ? UWB_ChargeLibrary::GetChargeFromHoldTime(HoldTime, DashChargeMinHoldTime, DashChargeTime) : 1.f;
}

float UWB_PlayerDashProfile::GetDistanceForCharge(float Charge) const
{
	return bUseChargedDash ? FMath::Lerp(DashWeakDistance, DashFullDistance, FMath::Clamp(Charge, 0.f, 1.f)) : DashFullDistance;
}

float UWB_PlayerDashProfile::GetDurationForCharge(float Charge) const
{
	return bUseChargedDash ? FMath::Lerp(DashWeakDuration, DashFullDuration, FMath::Clamp(Charge, 0.f, 1.f)) : DashFullDuration;
}

float UWB_PlayerDashProfile::GetDamageForCharge(float Charge) const
{
	return FMath::Lerp(DashWeakDamage, DashFullDamage, FMath::Clamp(Charge, 0.f, 1.f));
}

float UWB_PlayerDashProfile::GetAngleFactor(float Angle) const
{
	return FMath::GetMappedRangeValueClamped(FVector2D(DashHeadOnAngle, 90.f), FVector2D(1.f, DashGrazingDamageFactor), Angle);
}

void UWB_PlayerDashProfile::PostLoad()
{
	Super::PostLoad();
	RefreshPreview();
}

#if WITH_EDITOR
void UWB_PlayerDashProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshPreview();
}
#endif

int32 UWB_PlayerDashProfile::ComputePreviewHits(float Damage) const
{
	if (Damage <= 0.f || Damage < SimulatedWallMinDamage)
	{
		return 0;
	}
	// The first hit on an intact wall leaves at least 1 health (MaxStatesPerHit 1), then each hit removes Damage.
	const float HealthAfterFirst = FMath::Max(SimulatedWallHealth - Damage, 1.f);
	return 1 + FMath::CeilToInt(HealthAfterFirst / Damage);
}

void UWB_PlayerDashProfile::RefreshPreview()
{
	PreviewWeakSpeed = DashWeakDuration > UE_KINDA_SMALL_NUMBER ? DashWeakDistance / DashWeakDuration : 0.f;
	PreviewFullSpeed = DashFullDuration > UE_KINDA_SMALL_NUMBER ? DashFullDistance / DashFullDuration : 0.f;
	PreviewAngleFactor = GetAngleFactor(SimulatedDashAngle);

	PreviewWeakDamageHeadOn = DashWeakDamage;
	PreviewFullDamageHeadOn = DashFullDamage;
	PreviewWeakDamageAtAngle = DashWeakDamage * PreviewAngleFactor;
	PreviewFullDamageAtAngle = DashFullDamage * PreviewAngleFactor;
	PreviewWeakDamageGrazing = DashWeakDamage * DashGrazingDamageFactor;
	PreviewFullDamageGrazing = DashFullDamage * DashGrazingDamageFactor;

	PreviewHitsFullHeadOn = ComputePreviewHits(PreviewFullDamageHeadOn);
	PreviewHitsFullAtAngle = ComputePreviewHits(PreviewFullDamageAtAngle);
	PreviewHitsWeakHeadOn = ComputePreviewHits(PreviewWeakDamageHeadOn);
	PreviewHitsWeakAtAngle = ComputePreviewHits(PreviewWeakDamageAtAngle);
	PreviewHitsWeakGrazing = ComputePreviewHits(PreviewWeakDamageGrazing);
}
