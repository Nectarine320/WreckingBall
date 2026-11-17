#include "Impact/Destructible/WB_WobbleComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UWB_WobbleComponent::UWB_WobbleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = false;
}

bool UWB_WobbleComponent::IsWobbleEnabled(const FWB_WobbleSettings& Settings)
{
	return Settings.AngleLight > 0.f && Settings.DurationLight > 0.f;
}

bool UWB_WobbleComponent::ComputeTiltAxis(const USceneComponent* Pivot, const FVector& ImpactNormal, FVector& OutAxis) const
{
	// Hit normal in the pivot parent space (the frame the relative rotation lives in).
	const USceneComponent* Parent = Pivot->GetAttachParent();
	const FTransform ParentTransform = Parent ? Parent->GetComponentTransform()
		: (Pivot->GetOwner() ? Pivot->GetOwner()->GetActorTransform() : FTransform::Identity);
	const FVector LocalNormal = ParentTransform.InverseTransformVectorNoScale(ImpactNormal.GetSafeNormal());

	if (!bUseFourFaces)
	{
		// Same rule as the lot 9 wall: side A (dot > threshold) rolls by -angle, side B by +angle, edges do not wobble.
		const float SideDot = LocalNormal.Y;
		if (FMath::Abs(SideDot) <= SingleAxisMinSideDot)
		{
			return false;
		}
		const float Direction = SideDot > 0.f ? -1.f : 1.f;
		const FVector RollAxis = FRotator(0.f, 0.f, 1.f).Quaternion().GetRotationAxis();
		OutAxis = RollAxis * Direction;
		return true;
	}

	// Four faces: snap to the dominant horizontal face, then lean away from it, pivot at the base.
	const FVector Horizontal(LocalNormal.X, LocalNormal.Y, 0.f);
	if (Horizontal.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		return false;
	}
	const FVector Face = FMath::Abs(LocalNormal.X) >= FMath::Abs(LocalNormal.Y)
		? FVector(FMath::Sign(LocalNormal.X), 0.f, 0.f)
		: FVector(0.f, FMath::Sign(LocalNormal.Y), 0.f);
	const FVector Push = bInvertFourFacesDirection ? Face : -Face;

	FVector Axis = FVector::CrossProduct(FVector::UpVector, Push).GetSafeNormal();
	// Handedness-proof: a positive angle must tilt the top toward Push.
	if (FVector::DotProduct(FQuat(Axis, 0.1f).RotateVector(FVector::UpVector), Push) < 0.f)
	{
		Axis = -Axis;
	}
	OutAxis = Axis;
	return true;
}

USceneComponent* UWB_WobbleComponent::GetPivotForNormal(FVector ImpactNormal) const
{
	AActor* Owner = GetOwner();
	USceneComponent* Positive = Cast<USceneComponent>(PositiveSidePivot.GetComponent(Owner));
	if (bUseFourFaces || !Owner)
	{
		return Positive;
	}

	USceneComponent* Negative = Cast<USceneComponent>(NegativeSidePivot.GetComponent(Owner));
	if (!Negative)
	{
		return Positive;
	}

	// Same side test as the lot 9 wall: owner right vector.
	return FVector::DotProduct(ImpactNormal, Owner->GetActorRightVector()) >= 0.f ? Positive : Negative;
}

bool UWB_WobbleComponent::StartWobbleFromNormal(FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings)
{
	return StartWobbleFromImpact(GetPivotForNormal(ImpactNormal), ImpactNormal, Damage, Settings);
}

bool UWB_WobbleComponent::StartMinorHitWobbleFromNormal(FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings)
{
	if (!Settings.bWobbleOnMinorHits || Damage <= 0.f)
	{
		return false;
	}
	return StartWobbleFromNormal(ImpactNormal, Damage, Settings);
}

bool UWB_WobbleComponent::StartWobbleFromImpact(USceneComponent* Pivot, FVector ImpactNormal, float Damage, const FWB_WobbleSettings& Settings)
{
	if (!IsValid(Pivot) || !IsWobbleEnabled(Settings))
	{
		return false;
	}

	FVector Axis;
	if (!ComputeTiltAxis(Pivot, ImpactNormal, Axis))
	{
		return false;
	}

	float Alpha;
	if (Settings.DamageStrong > Settings.DamageLight)
	{
		Alpha = FMath::Clamp((Damage - Settings.DamageLight) / (Settings.DamageStrong - Settings.DamageLight), 0.f, 1.f);
	}
	else
	{
		Alpha = Damage >= Settings.DamageStrong ? 1.f : 0.f;
	}
	float AngleDeg = FMath::Lerp(Settings.AngleLight, Settings.AngleStrong, Alpha);
	float Duration = FMath::Lerp(Settings.DurationLight, Settings.DurationStrong, Alpha);

	// Minor hits: below DamageLight, scale the Light values down, from MinorHitMinScale at 0 damage to x1 at DamageLight.
	if (Settings.bWobbleOnMinorHits && Settings.DamageLight > 0.f && Damage < Settings.DamageLight)
	{
		const float MinScale = FMath::Clamp(Settings.MinorHitMinScale, 0.f, 1.f);
		const float Scale = FMath::Lerp(MinScale, 1.f, FMath::Clamp(Damage / Settings.DamageLight, 0.f, 1.f));
		AngleDeg *= Scale;
		Duration *= Scale;
	}
	const float Frequency = FMath::Lerp(Settings.FrequencyLight, Settings.FrequencyStrong, Alpha);
	if (Duration <= 0.f)
	{
		return false;
	}

	// Interrupt: every wobble in progress freezes its current pose, which then fades out. No jump.
	for (FWB_PivotWobble& State : States)
	{
		State.FadeFrom = Settings.InterruptFadeTime > 0.f ? ComputeOffset(State) : FQuat::Identity;
		State.FadeTime = Settings.InterruptFadeTime;
		State.FadeElapsed = 0.f;
		State.bOscillating = false;
	}

	FWB_PivotWobble* Target = States.FindByPredicate([Pivot](const FWB_PivotWobble& State) { return State.Pivot.Get() == Pivot; });
	if (!Target)
	{
		// First wobble on this pivot: its current rotation is the rest rotation it will always come back to.
		Target = &States.AddDefaulted_GetRef();
		Target->Pivot = Pivot;
		Target->Rest = Pivot->GetRelativeRotation().Quaternion();
	}

	Target->bOscillating = true;
	Target->Axis = Axis;
	Target->AmplitudeRad = FMath::DegreesToRadians(AngleDeg);
	Target->Duration = Duration;
	Target->Frequency = Frequency;
	Target->Elapsed = 0.f;

	SetComponentTickEnabled(true);
	return true;
}

FQuat UWB_WobbleComponent::ComputeOffset(const FWB_PivotWobble& State)
{
	FQuat Fade = FQuat::Identity;
	if (State.FadeTime > 0.f && State.FadeElapsed < State.FadeTime)
	{
		const float T = State.FadeElapsed / State.FadeTime;
		const float EaseOut = 1.f - FMath::Square(1.f - T);
		Fade = FQuat::Slerp(State.FadeFrom, FQuat::Identity, EaseOut);
	}

	FQuat Osc = FQuat::Identity;
	if (State.bOscillating)
	{
		const float Decay = FMath::Clamp(1.f - State.Elapsed / State.Duration, 0.f, 1.f);
		const float Angle = State.AmplitudeRad * FMath::Sin(UE_TWO_PI * State.Frequency * State.Elapsed) * Decay;
		Osc = FQuat(State.Axis, Angle);
	}

	return Osc * Fade;
}

bool UWB_WobbleComponent::IsFinished(const FWB_PivotWobble& State)
{
	const bool bFadeDone = State.FadeTime <= 0.f || State.FadeElapsed >= State.FadeTime;
	return !State.bOscillating && bFadeDone;
}

void UWB_WobbleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 Index = States.Num() - 1; Index >= 0; --Index)
	{
		FWB_PivotWobble& State = States[Index];
		USceneComponent* Pivot = State.Pivot.Get();
		if (!Pivot)
		{
			States.RemoveAtSwap(Index);
			continue;
		}

		State.FadeElapsed += DeltaTime;
		if (State.bOscillating)
		{
			State.Elapsed += DeltaTime;
			if (State.Elapsed >= State.Duration)
			{
				State.bOscillating = false;
			}
		}

		if (IsFinished(State))
		{
			// Always land exactly on the rest rotation, whatever the axes used on the way.
			Pivot->SetRelativeRotation(State.Rest);
			States.RemoveAtSwap(Index);
		}
		else
		{
			Pivot->SetRelativeRotation(ComputeOffset(State) * State.Rest);
		}
	}

	if (States.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UWB_WobbleComponent::StopAllWobbles()
{
	for (const FWB_PivotWobble& State : States)
	{
		if (USceneComponent* Pivot = State.Pivot.Get())
		{
			Pivot->SetRelativeRotation(State.Rest);
		}
	}
	States.Reset();
	SetComponentTickEnabled(false);
}

void UWB_WobbleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	States.Reset();
	Super::EndPlay(EndPlayReason);
}
