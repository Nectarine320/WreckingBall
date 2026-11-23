#include "Player/Hit/WB_PlayerHitComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Ball/Dribble/WB_BallDribbleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Feedback/WB_FeedbackLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/Hit/WB_PlayerHitProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerHit, Log, All);

UWB_PlayerHitComponent::UWB_PlayerHitComponent()
{
	// Ticks only during the Dash bounce slide.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWB_PlayerHitComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		PlayerState = Owner->FindComponentByClass<UWB_PlayerStateComponent>();

		// The capsule must report physics hits, or a thrown object hitting the player fires no Hit event.
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			Root->SetNotifyRigidBodyCollision(true);
		}

		// The Down timer starts when the Dash bump lands.
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			Character->LandedDelegate.AddDynamic(this, &UWB_PlayerHitComponent::HandleLanded);
		}
	}
	if (PlayerState)
	{
		PlayerState->OnPlayerStateChanged.AddDynamic(this, &UWB_PlayerHitComponent::HandlePlayerStateChanged);
	}
	else
	{
		UE_LOG(LogWBPlayerHit, Warning, TEXT("%s: UWB_PlayerHitComponent needs a UWB_PlayerStateComponent on the same actor."), *GetNameSafe(GetOwner()));
	}
}

const UWB_PlayerHitProfile* UWB_PlayerHitComponent::GetActiveHitProfile() const
{
	if (!HitProfile && !bWarnedNoProfile)
	{
		bWarnedNoProfile = true;
		UE_LOG(LogWBPlayerHit, Warning, TEXT("%s: no HitProfile on %s (DA_PlayerHit_Default expected). The player cannot be hit."), *GetNameSafe(GetOwner()), *GetName());
	}
	return HitProfile.Get();
}

bool UWB_PlayerHitComponent::IsInPostDownGrace() const
{
	// Invulnerable from the moment the player is knocked Down until PostDownGraceSeconds after getting up.
	if (PlayerState && PlayerState->IsInState(WBTags::Player_State_Down))
	{
		return true;
	}
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < GraceEndTime;
}

bool UWB_PlayerHitComponent::CanBeHitBy(AActor* Source, bool bAllowFriendlyFire) const
{
	AActor* Owner = GetOwner();
	if (!PlayerState || !IsValid(Source) || !Owner || Source == Owner)
	{
		return false;
	}
	if (IsInPostDownGrace())
	{
		return false;
	}

	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (const double* LastTime = LastHitTimes.Find(Source))
	{
		if (World && World->GetTimeSeconds() - *LastTime < Profile->HitCooldown)
		{
			return false;
		}
	}

	// Own thrown object: never. Teammate: only with friendly fire.
	if (const APawn* Responsible = UWB_FeedbackLibrary::GetResponsiblePawn(Source))
	{
		if (Responsible == Owner)
		{
			return false;
		}
		if (!bAllowFriendlyFire && TeamIndex >= 0)
		{
			const UWB_PlayerHitComponent* OtherHit = Responsible->FindComponentByClass<UWB_PlayerHitComponent>();
			if (OtherHit && OtherHit->TeamIndex == TeamIndex)
			{
				return false;
			}
		}
	}
	return true;
}

void UWB_PlayerHitComponent::RegisterHit(AActor* Source)
{
	if (const UWorld* World = GetWorld())
	{
		LastHitTimes.Add(Source, World->GetTimeSeconds());
	}
}

bool UWB_PlayerHitComponent::ReceiveThrownHit(AActor* Source, FVector ImpactVelocity, bool bSourceIsBall)
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile || (bSourceIsBall && !Profile->bBallCanStun))
	{
		return false;
	}
	// Only an object sent by a player (throw, drop on hit, Dash push: SetResponsiblePawn) can stun.
	// Debris, objects falling off a wall or pushed by other objects have no responsible player: no stun.
	if (!bSourceIsBall && !UWB_FeedbackLibrary::GetResponsiblePawn(Source))
	{
		return false;
	}
	if (ImpactVelocity.Size() < Profile->StunMinImpactSpeed || !CanBeHitBy(Source, Profile->bFriendlyFire))
	{
		return false;
	}
	// The get-up plays at the end of the state: the player stays Stunned (cannot move) until it is over.
	if (!PlayerState->ChangePlayerStateForDuration(WBTags::Player_State_Stunned, Profile->StunDuration + GetAnimLength(Profile->StunGetUpAnim)))
	{
		return false;
	}
	if (const UWorld* World = GetWorld())
	{
		HitReactStartTime = World->GetTimeSeconds();
		GetUpStartTime = HitReactStartTime + Profile->StunDuration;
	}

	RegisterHit(Source);
	UE_LOG(LogWBPlayerHit, Verbose, TEXT("%s stunned by %s (%.0f cm/s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Source), ImpactVelocity.Size());
	OnHitReceived.Broadcast(Source, WBTags::Player_State_Stunned, ImpactVelocity.GetSafeNormal2D());
	return true;
}

bool UWB_PlayerHitComponent::CanBeDashedBy(AActor* Dasher) const
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	return Profile && CanBeHitBy(Dasher, Profile->bDashFriendlyFire) && PlayerState->CanEnterState(WBTags::Player_State_Down);
}

bool UWB_PlayerHitComponent::ReceiveDashHitAndSteal(AActor* Dasher, FVector Direction, AActor*& OutStolenBall)
{
	OutStolenBall = nullptr;
	// Read before the hit: the knocked player drops the ball while ReceiveDashHit runs (OnHitReceived -> ForceDrop).
	const UWB_BallDribbleComponent* Dribble = GetOwner() ? GetOwner()->FindComponentByClass<UWB_BallDribbleComponent>() : nullptr;
	AActor* Ball = Dribble ? Dribble->GetDribbledBall() : nullptr;

	if (!ReceiveDashHit(Dasher, Direction))
	{
		return false;
	}
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	const UWB_PlayerStateComponent* DasherState = Dasher ? Dasher->FindComponentByClass<UWB_PlayerStateComponent>() : nullptr;
	if (Profile && Profile->bDashStealsBall && Ball && !(DasherState && DasherState->IsHoldingAnything()))
	{
		OutStolenBall = Ball;
	}
	return true;
}

bool UWB_PlayerHitComponent::ReceiveDashHit(AActor* Dasher, FVector Direction)
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile || !CanBeHitBy(Dasher, Profile->bDashFriendlyFire))
	{
		return false;
	}

	// Sideways bump: perpendicular to the Dash, toward the side of the Dash line where this player stands.
	// The dasher runs through (BP_GenericCharacter ignores the victim), so a push along the Dash would put the victim back in its way.
	const FVector DashDirection = Direction.GetSafeNormal2D();
	const FVector ToVictim = (GetOwner()->GetActorLocation() - Dasher->GetActorLocation()).GetSafeNormal2D();
	FVector PushDirection = ToVictim;
	if (!DashDirection.IsNearlyZero())
	{
		const FVector DashRight = FVector::CrossProduct(FVector::UpVector, DashDirection);
		float Side = FVector::DotProduct(ToVictim, DashRight);
		if (FMath::IsNearlyZero(Side, Profile->DownPushCenterTolerance))
		{
			Side = FMath::RandBool() ? 1.f : -1.f; // Dead center: pick a side.
		}
		PushDirection = DashRight * FMath::Sign(Side);
	}
	return EnterDownPushed(Dasher, PushDirection, Profile->DownKnockbackStrength, Profile->DownKnockbackUpStrength);
}

bool UWB_PlayerHitComponent::ReceiveSlamHit(AActor* Source, FVector PushDirection)
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile || !CanBeHitBy(Source, Profile->bSlamFriendlyFire))
	{
		return false;
	}
	FVector Push = PushDirection.GetSafeNormal2D();
	if (Push.IsNearlyZero() && Source)
	{
		Push = (GetOwner()->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
	}
	return EnterDownPushed(Source, Push, Profile->SlamKnockbackStrength, Profile->SlamKnockbackUpStrength);
}

bool UWB_PlayerHitComponent::EnterDownPushed(AActor* Source, FVector PushDirection, float Strength, float UpStrength)
{
	// Down without a timer yet: the timer starts on landing (HandleLanded), or right away when the bump has no jump.
	if (!PlayerState->ChangePlayerState(WBTags::Player_State_Down))
	{
		return false;
	}

	RegisterHit(Source);
	if (const UWorld* World = GetWorld())
	{
		HitReactStartTime = World->GetTimeSeconds();
	}
	GetUpStartTime = -1.0; // Known on landing.

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character)
	{
		// Face the push direction right away (the movement then keeps this orientation while sliding).
		Character->SetActorRotation(FRotator(0.f, PushDirection.Rotation().Yaw, 0.f));
		Character->LaunchCharacter(PushDirection * Strength + FVector::UpVector * UpStrength, true, true);
	}
	if (Character && UpStrength > 0.f)
	{
		bAwaitingDownLanding = true;
	}
	else
	{
		StartDownGroundTimer();
	}
	UE_LOG(LogWBPlayerHit, Verbose, TEXT("%s knocked down by %s."), *GetNameSafe(GetOwner()), *GetNameSafe(Source));
	OnHitReceived.Broadcast(Source, WBTags::Player_State_Down, PushDirection);
	return true;
}

bool UWB_PlayerHitComponent::ApplyDashRecoil(AActor* HitActor, FVector HitNormal, FVector DashDirection, float RunUpDistance, bool& bStunned)
{
	bStunned = false;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	// Only once per Dash: after the bounce the player is no longer Dashing, so later sweep hits do nothing.
	if (!PlayerState || !Character || !PlayerState->IsInState(WBTags::Player_State_Dashing))
	{
		return false;
	}
	// Another player is never a surface: the Dash on a player keeps its own rules (ReceiveDashHit).
	if (IsValid(HitActor) && HitActor->FindComponentByClass<UWB_PlayerHitComponent>())
	{
		return false;
	}
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile)
	{
		return false;
	}

	// Floor, ceiling or top of an edge: no bounce, the height only comes from RecoilUpSpeed.
	const FVector Normal = HitNormal.GetSafeNormal();
	if (Normal.IsNearlyZero() || FMath::Abs(Normal.Z) > Profile->RecoilMaxNormalZ)
	{
		return false;
	}
	const FVector FlatNormal = Normal.GetSafeNormal2D();
	FVector Direction = DashDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -FlatNormal;
	}

	// Flat angle between the Dash and the surface (inverse of its normal): 0 = head-on.
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, -FlatNormal), -1.f, 1.f)));
	const bool bHeadOn = Angle <= Profile->RecoilStunAngle;

	// State first: if the table refuses it, nothing moves.
	if (bHeadOn)
	{
		if (!PlayerState->ChangePlayerStateForDuration(WBTags::Player_State_Stunned, Profile->StunDuration + GetAnimLength(Profile->StunGetUpAnim)))
		{
			return false;
		}
		if (const UWorld* World = GetWorld())
		{
			HitReactStartTime = World->GetTimeSeconds();
			GetUpStartTime = HitReactStartTime + Profile->StunDuration;
		}
	}
	else if (!PlayerState->ChangePlayerStateForDuration(WBTags::Player_State_Recoiling, Profile->RecoilDuration))
	{
		return false;
	}

	// Mirror bounce, like a ball: the Dash direction reflected on the surface (45 degrees in, 45 degrees out on the other side of the normal).
	// Head-on, this is straight back along the normal. Always at least a little away from the surface.
	FVector Mirror = (Direction - 2.f * FVector::DotProduct(Direction, FlatNormal) * FlatNormal).GetSafeNormal2D();
	if (FVector::DotProduct(Mirror, FlatNormal) < FMath::Sin(FMath::DegreesToRadians(Profile->RecoilMinBounceAngle)))
	{
		Mirror = FlatNormal;
	}

	// The player turns away from the wall and faces the bounce direction (user decision 2026-11-16).
	Character->SetActorRotation(FRotator(0.f, Mirror.Rotation().Yaw, 0.f));

	// Stop the Dash speed, then only a small hop: the horizontal slide is scripted in TickComponent (no stacking, exact distance).
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (Profile->RecoilUpSpeed > 0.f)
	{
		Character->LaunchCharacter(FVector::UpVector * Profile->RecoilUpSpeed, true, true);
	}

	// Bounce as strong as the impact: a fraction of the distance the Dash travelled before hitting.
	BounceState = bHeadOn ? WBTags::Player_State_Stunned : WBTags::Player_State_Recoiling;
	BounceStart = Character->GetActorLocation();
	BounceDirection = Mirror;
	BounceDistance = FMath::Max(FMath::Max(RunUpDistance, 0.f) * Profile->RecoilDistanceRatio, Profile->RecoilMinDistance);
	BounceElapsed = 0.f;
	BounceDuration = FMath::Max(Profile->RecoilDuration, 0.05f);
	bBouncing = true;
	SetComponentTickEnabled(true);

	bStunned = bHeadOn;
	UE_LOG(LogWBPlayerHit, Verbose, TEXT("%s bounced off %s (%.0f deg, %s)."), *GetNameSafe(Character), *GetNameSafe(HitActor), Angle, bHeadOn ? TEXT("stunned") : TEXT("recoiling"));
	if (bHeadOn)
	{
		OnHitReceived.Broadcast(HitActor, WBTags::Player_State_Stunned, Mirror);
	}
	return true;
}

void UWB_PlayerHitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!bBouncing || !Owner)
	{
		StopBounce();
		return;
	}
	// Fast start, slowing down at the end (ease out), along the mirrored direction. Height is left to the movement component (hop, gravity).
	BounceElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(BounceElapsed / BounceDuration, 0.f, 1.f);
	const float Eased = 1.f - FMath::Square(1.f - Alpha);
	const FVector Target = BounceStart + BounceDirection * (BounceDistance * Eased);
	const FVector Current = Owner->GetActorLocation();

	FHitResult Hit;
	Owner->SetActorLocation(FVector(Target.X, Target.Y, Current.Z), true, &Hit);
	// Blocked by another wall or object: the bounce ends there.
	if (Hit.bBlockingHit || Alpha >= 1.f)
	{
		StopBounce();
	}
}

void UWB_PlayerHitComponent::StopBounce()
{
	bBouncing = false;
	SetComponentTickEnabled(false);
}

FVector UWB_PlayerHitComponent::EstimatePreImpactVelocity(UPrimitiveComponent* OtherComp, FVector NormalImpulse)
{
	if (!IsValid(OtherComp))
	{
		return FVector::ZeroVector;
	}
	const FVector Velocity = OtherComp->GetComponentVelocity();
	if (!OtherComp->IsSimulatingPhysics())
	{
		return Velocity;
	}
	const FVector DeltaV = NormalImpulse / FMath::Max(OtherComp->GetMass(), 0.01f);
	const FVector WithPlus = Velocity + DeltaV;
	const FVector WithMinus = Velocity - DeltaV;
	return WithPlus.SizeSquared() >= WithMinus.SizeSquared() ? WithPlus : WithMinus;
}

void UWB_PlayerHitComponent::HandlePlayerStateChanged(FGameplayTag OldState, FGameplayTag NewState)
{
	if (bBouncing && OldState == BounceState)
	{
		// The bounce state was cut short (knocked Down, Disabled...): the slide stops too.
		StopBounce();
	}
	if (NewState == WBTags::Player_State_Down)
	{
		// Invulnerable (and blinking) from the knock down.
		StartGraceBlink();
	}
	if (OldState == WBTags::Player_State_Down || OldState == WBTags::Player_State_Stunned)
	{
		// The hit reaction is over (or replaced: Stunned -> Down sets new times right after this call).
		HitReactStartTime = -1.0;
		GetUpStartTime = -1.0;
	}
	if (OldState == WBTags::Player_State_Down)
	{
		bAwaitingDownLanding = false;
		const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
		if (const UWorld* World = GetWorld(); World && Profile)
		{
			GraceEndTime = World->GetTimeSeconds() + Profile->PostDownGraceSeconds;
		}
		StartGraceBlink(); // Keeps blinking if already started (nothing restarts), until the end of the grace.
	}
}

void UWB_PlayerHitComponent::StartGraceBlink()
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Profile || !Profile->GraceBlinkMaterial || !Character || !Character->GetMesh() || !World || !IsInPostDownGrace())
	{
		return;
	}
	if (World->GetTimerManager().IsTimerActive(GraceBlinkTimer))
	{
		return; // Already blinking: the timer stops by itself when the grace is over.
	}
	OverlayBeforeBlink = Character->GetMesh()->GetOverlayMaterial();
	bGraceFlashOn = false;
	TickGraceBlink();
	World->GetTimerManager().SetTimer(GraceBlinkTimer, FTimerDelegate::CreateUObject(this, &UWB_PlayerHitComponent::TickGraceBlink),
		Profile->GraceBlinkInterval, true);
}

void UWB_PlayerHitComponent::TickGraceBlink()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile || !Character || !Character->GetMesh() || !IsInPostDownGrace())
	{
		StopGraceBlink();
		return;
	}
	bGraceFlashOn = !bGraceFlashOn;
	Character->GetMesh()->SetOverlayMaterial(bGraceFlashOn ? Profile->GraceBlinkMaterial.Get() : OverlayBeforeBlink.Get());
}

void UWB_PlayerHitComponent::StopGraceBlink()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GraceBlinkTimer);
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (Character->GetMesh())
		{
			Character->GetMesh()->SetOverlayMaterial(OverlayBeforeBlink);
		}
	}
	bGraceFlashOn = false;
	OverlayBeforeBlink = nullptr;
}

void UWB_PlayerHitComponent::HandleLanded(const FHitResult& Hit)
{
	if (!bAwaitingDownLanding)
	{
		return;
	}
	bAwaitingDownLanding = false;
	// Only if still Down (a GameMode change, e.g. Disabled, cancels the landing timer).
	if (PlayerState && PlayerState->IsInState(WBTags::Player_State_Down))
	{
		StartDownGroundTimer();
	}
}

void UWB_PlayerHitComponent::StartDownGroundTimer()
{
	// On the ground: lie for DownDuration, then get up (the get-up length is still Down: invulnerable, cannot move).
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile)
	{
		return;
	}
	PlayerState->ReturnToFreeAfter(Profile->DownDuration + GetAnimLength(Profile->DownGetUpAnim));
	if (const UWorld* World = GetWorld())
	{
		GetUpStartTime = World->GetTimeSeconds() + Profile->DownDuration;
	}
}

float UWB_PlayerHitComponent::GetAnimLength(const UAnimSequenceBase* Anim)
{
	// Real playing time: the asset's RateScale speeds it up or slows it down (e.g. cartoon-fast stun at RateScale 2).
	return Anim ? Anim->GetPlayLength() / FMath::Max(FMath::Abs(Anim->RateScale), 0.01f) : 0.f;
}

EWB_HitReactPhase UWB_PlayerHitComponent::GetHitReactPhase() const
{
	if (!PlayerState)
	{
		return EWB_HitReactPhase::None;
	}
	const bool bDown = PlayerState->IsInState(WBTags::Player_State_Down);
	if (!bDown && !PlayerState->IsInState(WBTags::Player_State_Stunned))
	{
		return EWB_HitReactPhase::None;
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (GetUpStartTime >= 0.0 && Now >= GetUpStartTime)
	{
		return EWB_HitReactPhase::GetUp;
	}
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile)
	{
		return EWB_HitReactPhase::Loop;
	}
	const float EnterLength = GetAnimLength(bDown ? Profile->DownEnterAnim : Profile->StunEnterAnim);
	if (Now - HitReactStartTime < EnterLength || (bDown && bAwaitingDownLanding))
	{
		return EWB_HitReactPhase::Enter;
	}
	return EWB_HitReactPhase::Loop;
}

FVector UWB_PlayerHitComponent::MakeDropImpulse(FVector HitDirection) const
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	if (!Profile)
	{
		return FVector::ZeroVector;
	}
	FVector Direction = HitDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero() && GetOwner())
	{
		Direction = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	}
	Direction = Direction.RotateAngleAxis(FMath::FRandRange(-Profile->DropRandomYaw, Profile->DropRandomYaw), FVector::UpVector);
	return Direction * Profile->DropSpeed + FVector::UpVector * Profile->DropUpSpeed;
}
