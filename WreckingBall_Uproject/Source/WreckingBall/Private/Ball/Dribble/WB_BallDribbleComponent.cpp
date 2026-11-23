#include "Ball/Dribble/WB_BallDribbleComponent.h"

#include "Ball/Dribble/WB_BallDribbleProfile.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/State/WB_PlayerStateComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBBallDribble, Log, All);

UWB_BallDribbleComponent::UWB_BallDribbleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The push is read by the physics step: apply it just before.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UWB_BallDribbleComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	Character = Cast<ACharacter>(Owner);
	PlayerState = Owner ? Owner->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
}

void UWB_BallDribbleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDribble();
	Super::EndPlay(EndPlayReason);
}

bool UWB_BallDribbleComponent::StartDribble(AActor* Ball, UWB_BallDribbleProfile* BallProfile)
{
	UWB_BallDribbleProfile* Profile = BallProfile ? BallProfile : DefaultProfile.Get();
	UPrimitiveComponent* Body = UWB_BallPossessionSubsystem::FindBallBody(Ball);
	if (!Profile || !Character || !Body || !Body->IsSimulatingPhysics())
	{
		UE_LOG(LogWBBallDribble, Warning, TEXT("%s: StartDribble needs a profile (ball DribbleProfile or DefaultProfile), a Character owner and a simulated ball (%s). Ball not dribbled."),
			*GetPathNameSafe(this), *GetNameSafe(Ball));
		return false;
	}

	const AActor* Possessor = UWB_BallPossessionSubsystem::GetBallPossessor(this, Ball);
	if (Possessor && Possessor != Character)
	{
		return false;
	}

	if (IsDribbling() && DribbledBall.Get() != Ball)
	{
		StopDribble();
	}

	DribbledBall = Ball;
	DribbledBody = Body;
	ActiveProfile = Profile;
	TimeBeyondLoseDistance = 0.f;
	LastTouchTime = -1000.0;
	DribbleStartTime = GetWorld()->GetTimeSeconds();

	// The player's own capsule must not be blocked by (nor push) the ball it dribbles. Opponents still collide with it.
	Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Ball, true);
	const bool bNewPossession = Possessor != Character;
	UWB_BallPossessionSubsystem::SetBallDribbler(this, Ball, Character);
	SetComponentTickEnabled(true);

	if (bNewPossession && Profile->CatchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Profile->CatchSound, Ball->GetActorLocation(), Profile->CatchSoundVolume);
	}
	return true;
}

void UWB_BallDribbleComponent::StopDribble()
{
	AActor* Ball = DribbledBall.Get();
	if (Ball)
	{
		if (Character)
		{
			Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Ball, false);
		}
		UWB_BallPossessionSubsystem::ClearBallDribbler(this, Ball, Character);
	}
	DribbledBall.Reset();
	DribbledBody.Reset();
	ActiveProfile = nullptr;
	SetComponentTickEnabled(false);
}

void UWB_BallDribbleComponent::LoseBall(const TCHAR* Reason)
{
	AActor* Ball = DribbledBall.Get();
	if (bDebugLogLoss)
	{
		UE_LOG(LogWBBallDribble, Log, TEXT("%s lost %s: %s"), *GetNameSafe(Character), *GetNameSafe(Ball), Reason);
	}
	StopDribble();
	OnDribbleLost.Broadcast(Ball);
}

bool UWB_BallDribbleComponent::IsBallOnGround(const UPrimitiveComponent* Body, float BallRadius) const
{
	const FVector Center = Body->Bounds.Origin;
	const FVector End = Center - FVector(0.f, 0.f, BallRadius + ActiveProfile->GroundTolerance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBBallDribbleGround), false);
	Params.AddIgnoredActor(Body->GetOwner());
	Params.AddIgnoredActor(Character);
	FHitResult Hit;
	// Visibility: the gym floors are WorldDynamic, a WorldStatic test would miss them.
	return GetWorld()->LineTraceSingleByChannel(Hit, Center, End, ECC_Visibility, Params);
}

FVector UWB_BallDribbleComponent::GetPathPoint(const FVector& BallCenter, const FVector& Target, const FVector& PlayerCenter, float KeepOutRadius, const FVector& Forward) const
{
	// Closest point of the flat segment ball -> target to the player axis.
	const FVector Start(BallCenter.X, BallCenter.Y, 0.f);
	const FVector End(Target.X, Target.Y, 0.f);
	const FVector Axis(PlayerCenter.X, PlayerCenter.Y, 0.f);
	const FVector Closest = FMath::ClosestPointOnSegment(Axis, Start, End);
	const FVector FromAxis = Closest - Axis;
	if (FromAxis.SizeSquared() >= FMath::Square(KeepOutRadius))
	{
		return Target;
	}

	// The straight way crosses the capsule: head to a point beside it, on the ball's side.
	FVector Side = FromAxis;
	if (Side.IsNearlyZero())
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		Side = FVector::DotProduct(Start - Axis, Right) >= 0.f ? Right : -Right;
	}
	const FVector Waypoint = Axis + Side.GetSafeNormal() * KeepOutRadius;
	return FVector(Waypoint.X, Waypoint.Y, Target.Z);
}

void UWB_BallDribbleComponent::PutBallAtFeet(UPrimitiveComponent* Body, const FVector& DribblePoint, const FVector& PlayerCenter, float FeetZ, float BallRadius, const FVector& PlayerFlatVelocity)
{
	AActor* Ball = Body->GetOwner();
	// Ball center just above the floor. 1 cm: technical lift so the ball does not start inside the floor.
	const float CenterZ = FeetZ + BallRadius + 1.f;
	const FVector Start(PlayerCenter.X, PlayerCenter.Y, CenterZ);
	const FVector End(DribblePoint.X, DribblePoint.Y, CenterZ);

	// Swept with the ball's own collision, from the player to the dribble point: a wall in front stops the ball before it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBBallDribbleLeash), false);
	Params.AddIgnoredActor(Ball);
	Params.AddIgnoredActor(Character);
	FCollisionResponseParams Responses(Body->GetCollisionResponseToChannels());
	FHitResult Hit;
	FVector Destination = End;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Body->GetCollisionObjectType(), FCollisionShape::MakeSphere(BallRadius), Params, Responses))
	{
		Destination = Hit.bStartPenetrating ? Start : Hit.Location;
	}

	const FVector Delta = Destination - Body->Bounds.Origin;
	Ball->SetActorLocation(Ball->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
	Body->SetPhysicsLinearVelocity(PlayerFlatVelocity);
	Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	TimeBeyondLoseDistance = 0.f;
	if (bDebugLogLoss)
	{
		UE_LOG(LogWBBallDribble, Log, TEXT("%s: %s put back at the feet (locked possession)."), *GetNameSafe(Character), *GetNameSafe(Ball));
	}
}

void UWB_BallDribbleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UPrimitiveComponent* Body = DribbledBody.Get();
	const UWB_BallDribbleProfile* Profile = ActiveProfile;
	if (!Body || !Profile || !Character || !Body->IsSimulatingPhysics() || (PlayerState && !PlayerState->IsHoldingAnything()))
	{
		StopDribble();
		return;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector PlayerCenter = Capsule->GetComponentLocation();
	const FVector Forward = Capsule->GetForwardVector().GetSafeNormal2D();
	const float FeetZ = PlayerCenter.Z - Capsule->GetScaledCapsuleHalfHeight();
	const FVector PlayerVelocity = Movement ? Movement->Velocity : Character->GetVelocity();
	const FVector PlayerFlatVelocity(PlayerVelocity.X, PlayerVelocity.Y, 0.f);

	// Ball center: the bounds origin (the soccer ball's pivot is at its base).
	const FVector BallCenter = Body->Bounds.Origin;
	const float BallRadius = Body->Bounds.SphereRadius;
	const float KeepOutRadius = Capsule->GetScaledCapsuleRadius() + BallRadius + Profile->CapsuleClearance;

	// Dribble point: just in front of the feet, ahead by the player's speed.
	const float FrontDistance = Capsule->GetScaledCapsuleRadius() + BallRadius + Profile->FrontGap;
	FVector DribblePoint = PlayerCenter + Forward * FrontDistance + PlayerFlatVelocity * Profile->LeadTime;
	DribblePoint.Z = BallCenter.Z;

	// Loss: too high above the feet, or too far from the dribble point for too long.
	const float BallBottomHeight = BallCenter.Z - BallRadius - FeetZ;
	const FVector Gap = FVector(DribblePoint.X - BallCenter.X, DribblePoint.Y - BallCenter.Y, 0.f);
	const float GapSize = Gap.Size();
	// Just after a catch (CatchHeightGrace), a ball still in the air is kept: gravity brings it down to the feet.
	const bool bHeightGrace = GetWorld()->GetTimeSeconds() - DribbleStartTime < Profile->CatchHeightGrace;

	if (Profile->bLockPossession)
	{
		// Locked: never lost by itself. Too far or too high (stuck, bumped): back at the feet.
		if (GapSize > Profile->LockLeashDistance || (BallBottomHeight > Profile->LockMaxHeight && !bHeightGrace))
		{
			PutBallAtFeet(Body, DribblePoint, PlayerCenter, FeetZ, BallRadius, PlayerFlatVelocity);
			return;
		}
		// The velocity is set, not pushed: no inertia, the ball follows every turn. The height stays to gravity and the floor.
		const FVector PathPoint = GetPathPoint(BallCenter, DribblePoint, PlayerCenter, KeepOutRadius, Forward);
		const FVector PathGap(PathPoint.X - BallCenter.X, PathPoint.Y - BallCenter.Y, 0.f);
		const FVector TargetVelocity = PlayerFlatVelocity + PathGap * Profile->FollowStiffness;
		const FVector BallVelocity = Body->GetPhysicsLinearVelocity();
		Body->SetPhysicsLinearVelocity(FVector(TargetVelocity.X, TargetVelocity.Y, BallVelocity.Z));

		if (Profile->RollFollowSpeed > 0.f && BallRadius > UE_KINDA_SMALL_NUMBER && IsBallOnGround(Body, BallRadius))
		{
			const FVector TargetSpin = FVector::CrossProduct(FVector::UpVector, FVector(TargetVelocity.X, TargetVelocity.Y, 0.f)) / BallRadius;
			const FVector Spin = Body->GetPhysicsAngularVelocityInRadians();
			const float Alpha = 1.f - FMath::Exp(-Profile->RollFollowSpeed * DeltaTime);
			Body->SetPhysicsAngularVelocityInRadians(FMath::Lerp(Spin, TargetSpin, Alpha));
		}

#if ENABLE_DRAW_DEBUG
		if (bDebugDrawDribble)
		{
			UWorld* World = GetWorld();
			DrawDebugSphere(World, DribblePoint, 6.f, 8, FColor::Green, false, 0.f);
			DrawDebugSphere(World, PathPoint, 4.f, 8, FColor::Cyan, false, 0.f);
			DrawDebugCircle(World, DribblePoint, Profile->LockLeashDistance, 32, FColor::Purple, false, 0.f, 0, 1.f, FVector::ForwardVector, FVector::RightVector, false);
		}
#endif
		return;
	}

	if (BallBottomHeight > Profile->LoseHeight && !bHeightGrace)
	{
		LoseBall(*FString::Printf(TEXT("height %.0f cm > LoseHeight %.0f"), BallBottomHeight, Profile->LoseHeight));
		return;
	}
	TimeBeyondLoseDistance = GapSize > Profile->LoseDistance ? TimeBeyondLoseDistance + DeltaTime : 0.f;
	if (TimeBeyondLoseDistance > Profile->LoseDelay)
	{
		LoseBall(*FString::Printf(TEXT("distance %.0f cm > LoseDistance %.0f for %.2f s"), GapSize, Profile->LoseDistance, TimeBeyondLoseDistance));
		return;
	}

	const FVector BallVelocity = Body->GetPhysicsLinearVelocity();
	const FVector BallFlatVelocity(BallVelocity.X, BallVelocity.Y, 0.f);
	const FVector PathPoint = GetPathPoint(BallCenter, DribblePoint, PlayerCenter, KeepOutRadius, Forward);
	const FVector PathGap(PathPoint.X - BallCenter.X, PathPoint.Y - BallCenter.Y, 0.f);

	FVector Acceleration = FVector::ZeroVector;
	const bool bTouches = Profile->bUseTouches && PlayerFlatVelocity.Size() >= Profile->TouchMinPlayerSpeed;
	if (bTouches)
	{
		// Touch: the player caught up with the ball (ball at or behind the dribble point, along the facing). Send it ahead.
		const float BallAhead = FVector::DotProduct(FVector(BallCenter.X - PlayerCenter.X, BallCenter.Y - PlayerCenter.Y, 0.f), Forward);
		const double Now = GetWorld()->GetTimeSeconds();
		if (BallAhead <= FrontDistance && Now - LastTouchTime >= Profile->TouchMinInterval)
		{
			LastTouchTime = Now;
			const FVector LeadPoint = DribblePoint + Forward * Profile->TouchLeadDistance;
			const FVector Direction = FVector(LeadPoint.X - BallCenter.X, LeadPoint.Y - BallCenter.Y, 0.f).GetSafeNormal();
			const FVector TouchVelocity = Direction * (PlayerFlatVelocity.Size() * Profile->TouchSpeedFactor);
			Body->SetPhysicsLinearVelocity(FVector(TouchVelocity.X, TouchVelocity.Y, BallVelocity.Z));
		}
		else
		{
			// Between touches: only the side drift is corrected, the ball keeps its forward run.
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
			const float SideGap = FVector::DotProduct(PathGap, Right);
			const float SideSpeed = FVector::DotProduct(BallFlatVelocity - PlayerFlatVelocity, Right);
			Acceleration = Right * ((SideGap * Profile->FollowStiffness - SideSpeed) * Profile->FollowResponse);
		}
	}
	else
	{
		// Continuous follow: target speed = player's speed + gap x stiffness, reached at FollowResponse.
		const FVector TargetVelocity = PlayerFlatVelocity + PathGap * Profile->FollowStiffness;
		Acceleration = (TargetVelocity - BallFlatVelocity) * Profile->FollowResponse;
	}
	Acceleration = Acceleration.GetClampedToMaxSize(Profile->MaxAcceleration);

	// Elastic tether (test, off by default): beyond TetherDistance, an extra pull toward the dribble point.
	if (Profile->bUseTether && Profile->TetherStiffness > 0.f && GapSize > Profile->TetherDistance)
	{
		const FVector Pull = Gap.GetSafeNormal() * ((GapSize - Profile->TetherDistance) * Profile->TetherStiffness);
		Acceleration += Pull.GetClampedToMaxSize(Profile->TetherMaxAcceleration);
	}

	// Flat only: gravity and the floor handle the height.
	Acceleration.Z = 0.f;
	if (!Acceleration.IsNearlyZero())
	{
		Body->AddForce(Acceleration, NAME_None, true);
	}

	// Roll: on the floor, the spin follows (Up x flat velocity) / radius, so the ball rolls without sliding.
	if (Profile->RollFollowSpeed > 0.f && BallRadius > UE_KINDA_SMALL_NUMBER && IsBallOnGround(Body, BallRadius))
	{
		const FVector TargetSpin = FVector::CrossProduct(FVector::UpVector, BallFlatVelocity) / BallRadius;
		const FVector Spin = Body->GetPhysicsAngularVelocityInRadians();
		const float Alpha = 1.f - FMath::Exp(-Profile->RollFollowSpeed * DeltaTime);
		Body->SetPhysicsAngularVelocityInRadians(FMath::Lerp(Spin, TargetSpin, Alpha));
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawDribble)
	{
		UWorld* World = GetWorld();
		DrawDebugSphere(World, DribblePoint, 6.f, 8, FColor::Green, false, 0.f);
		DrawDebugSphere(World, PathPoint, 4.f, 8, FColor::Cyan, false, 0.f);
		DrawDebugLine(World, BallCenter, BallCenter + Acceleration * 0.02f, FColor::Yellow, false, 0.f, 0, 1.5f);
		DrawDebugCircle(World, DribblePoint, Profile->LoseDistance, 32, FColor::Red, false, 0.f, 0, 1.f, FVector::ForwardVector, FVector::RightVector, false);
		if (Profile->bUseTether)
		{
			DrawDebugCircle(World, DribblePoint, Profile->TetherDistance, 32, FColor::Orange, false, 0.f, 0, 1.f, FVector::ForwardVector, FVector::RightVector, false);
		}
	}
#endif
}
