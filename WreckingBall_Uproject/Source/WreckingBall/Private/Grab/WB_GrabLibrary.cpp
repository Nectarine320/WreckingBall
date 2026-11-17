#include "Grab/WB_GrabLibrary.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

bool UWB_GrabLibrary::IsLargeHeldObject(const UPrimitiveComponent* Held, float MinBoundsRadius)
{
	return IsValid(Held) && Held->Bounds.SphereRadius >= MinBoundsRadius;
}

FVector UWB_GrabLibrary::GetHandsMidpoint(const USkeletalMeshComponent* Mesh, FName LeftForearmBone, FName RightForearmBone,
	float ForearmLength, const FVector& LocalForearmDirection)
{
	if (!IsValid(Mesh))
	{
		return FVector::ZeroVector;
	}
	const FVector Direction = LocalForearmDirection.GetSafeNormal();
	auto HandTip = [&](FName Bone)
	{
		const FTransform BoneTransform = Mesh->GetSocketTransform(Bone, RTS_World);
		// The bone scale (100 on this Blender export) is ignored: the length is in world centimeters.
		return BoneTransform.GetLocation() + BoneTransform.TransformVectorNoScale(Direction) * ForearmLength;
	};
	return (HandTip(LeftForearmBone) + HandTip(RightForearmBone)) * 0.5f;
}

FVector UWB_GrabLibrary::GetThrowArcTarget(FVector CarryTarget, FVector HandsMidpoint, float Charge, float ArcHeight,
	float BlendInCharge, const FVector& Up)
{
	Charge = FMath::Clamp(Charge, 0.f, 1.f);
	// Smooth slide from the carry position to the hands at the start of the windup.
	const float BlendIn = BlendInCharge > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.f, 1.f, Charge / BlendInCharge) : 1.f;
	// Arc bump: zero at the start and at full charge, highest in the middle.
	const FVector Arc = Up.GetSafeNormal() * (ArcHeight * FMath::Sin(PI * Charge));
	return FMath::Lerp(CarryTarget, HandsMidpoint, BlendIn) + Arc;
}

FVector UWB_GrabLibrary::GetOverheadThrowTarget(FVector CarryTarget, const UCapsuleComponent* Capsule, const UPrimitiveComponent* Held, float Charge,
	float ArcHeight, float OverheadHeight, float OverheadForward, float ClearanceMargin)
{
	if (!IsValid(Capsule))
	{
		return CarryTarget;
	}
	const FVector Center = Capsule->GetComponentLocation();
	const FVector Up = Capsule->GetUpVector();
	const FVector Forward = FVector::VectorPlaneProject(Capsule->GetForwardVector(), Up).GetSafeNormal();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();

	// Real half-size of the held object along a world direction: its oriented box (local bounds, scaled and rotated)
	// projected on that direction. Much tighter than the bounding sphere for long or flat objects.
	FVector LocalExtent = FVector::ZeroVector;
	FTransform HeldTransform = FTransform::Identity;
	if (IsValid(Held))
	{
		HeldTransform = Held->GetComponentTransform();
		LocalExtent = Held->CalcBounds(FTransform::Identity).BoxExtent * HeldTransform.GetScale3D().GetAbs();
	}
	auto HalfSizeAlong = [&](const FVector& WorldDirection)
	{
		const FVector Local = HeldTransform.InverseTransformVectorNoScale(WorldDirection);
		return LocalExtent.X * FMath::Abs(Local.X) + LocalExtent.Y * FMath::Abs(Local.Y) + LocalExtent.Z * FMath::Abs(Local.Z);
	};
	const float HalfHeightOfObject = HalfSizeAlong(Up);
	const float TopOfHead = HalfHeight + HalfHeightOfObject;

	// Curve: carry position -> raised -> over the head (slightly forward, just above the head by the object's own half height).
	// No forward push: the keep-out below holds the object right against the body, at a distance set by its size.
	const FVector Start = CarryTarget;
	const FVector End = Center + Up * (TopOfHead + OverheadHeight) + Forward * OverheadForward;
	const FVector Control = (Start + End) * 0.5f + Up * ArcHeight;

	const float T = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Charge, 0.f, 1.f));
	const float U = 1.f - T;
	FVector Target = Start * (U * U) + Control * (2.f * U * T) + End * (T * T);

	// Keep the object out of the thrower: below the top of the head, stay outside the capsule cylinder by the object's
	// half size toward the player + ClearanceMargin.
	const FVector FromCenter = Target - Center;
	const float Height = FVector::DotProduct(FromCenter, Up);
	if (Height < TopOfHead)
	{
		FVector Flat = FromCenter - Up * Height;
		const FVector FlatDirection = Flat.IsNearlyZero() ? Forward : Flat.GetSafeNormal();
		const float KeepOutRadius = CapsuleRadius + HalfSizeAlong(FlatDirection) + ClearanceMargin;
		if (Flat.Size() < KeepOutRadius)
		{
			Target = Center + Up * Height + FlatDirection * KeepOutRadius;
		}
	}
	return Target;
}
