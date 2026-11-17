#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WB_GrabLibrary.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;

/**
 * Helpers for held objects (BPC_GrabObject): during a throw windup, a large object follows the point between the hands of the
 * thrower along an arc instead of staying at its carry position.
 */
UCLASS()
class WRECKINGBALL_API UWB_GrabLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when Held is large enough to be thrown with both hands: its bounding sphere radius (cm) is at least MinBoundsRadius. */
	UFUNCTION(BlueprintPure, Category = "Grab")
	static bool IsLargeHeldObject(const UPrimitiveComponent* Held, float MinBoundsRadius = 30.f);

	/**
	 * World point between the two hands of Mesh. The prototype skeleton has no hand bones: each hand is the tip of a forearm bone
	 * (LeftForearmBone / RightForearmBone), ForearmLength cm along the bone's local LocalForearmDirection. Mesh invalid: zero vector.
	 */
	UFUNCTION(BlueprintPure, Category = "Grab", meta = (AutoCreateRefTerm = "LocalForearmDirection"))
	static FVector GetHandsMidpoint(const USkeletalMeshComponent* Mesh, FName LeftForearmBone = "Arm2_l", FName RightForearmBone = "Arm2_r",
		float ForearmLength = 17.f, const FVector& LocalForearmDirection = FVector(0.f, -1.f, 0.f));

	/**
	 * Target of a large held object during a throw windup. Charge: windup progress 0..1 (ActionCharge).
	 * The target slides from CarryTarget to HandsMidpoint over the first BlendInCharge of the charge, and an arc bump is added on top:
	 * ArcHeight cm along Up at mid-charge (sine), so the object travels on a curve, never in a straight line.
	 */
	UFUNCTION(BlueprintPure, Category = "Grab", meta = (AutoCreateRefTerm = "Up"))
	static FVector GetThrowArcTarget(FVector CarryTarget, FVector HandsMidpoint, float Charge, float ArcHeight = 25.f,
		float BlendInCharge = 0.2f, const FVector& Up = FVector(0.f, 0.f, 1.f));

	/**
	 * Target of a large held object during a throw windup, independent of the animation. Charge: windup progress 0..1 (ActionCharge).
	 * The object travels on a curve (quadratic Bezier, eased) from CarryTarget, up, to an end point over the head: top of Capsule + Held's half height
	 * + OverheadHeight cm, OverheadForward cm in front of the capsule axis. ArcHeight cm raises the middle of the curve.
	 * Held's size is its oriented box (not its bounding sphere), measured along each direction. Below the top of the head, the target is pushed out
	 * of the capsule cylinder (capsule radius + Held's half size toward the thrower + ClearanceMargin cm), so the object stays close but never touches.
	 * Invalid Capsule: CarryTarget.
	 */
	UFUNCTION(BlueprintPure, Category = "Grab")
	static FVector GetOverheadThrowTarget(FVector CarryTarget, const UCapsuleComponent* Capsule, const UPrimitiveComponent* Held, float Charge,
		float ArcHeight = 25.f, float OverheadHeight = 15.f, float OverheadForward = 10.f, float ClearanceMargin = 5.f);
};
