#include "Player/Hit/WB_PlayerHitComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Feedback/WB_FeedbackLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Player/Hit/WB_PlayerHitProfile.h"
#include "Player/State/WB_PlayerStateComponent.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerHit, Log, All);

UWB_PlayerHitComponent::UWB_PlayerHitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	return HitProfile ? HitProfile.Get() : GetDefault<UWB_PlayerHitProfile>();
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

bool UWB_PlayerHitComponent::CanBeHitBy(AActor* Source) const
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
		if (!Profile->bFriendlyFire && TeamIndex >= 0)
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
	if (bSourceIsBall && !Profile->bBallCanStun)
	{
		return false;
	}
	if (ImpactVelocity.Size() < Profile->StunMinImpactSpeed || !CanBeHitBy(Source))
	{
		return false;
	}
	if (!PlayerState->ChangePlayerStateForDuration(WBTags::Player_State_Stunned, Profile->StunDuration))
	{
		return false;
	}

	RegisterHit(Source);
	UE_LOG(LogWBPlayerHit, Verbose, TEXT("%s stunned by %s (%.0f cm/s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Source), ImpactVelocity.Size());
	OnHitReceived.Broadcast(Source, WBTags::Player_State_Stunned, ImpactVelocity.GetSafeNormal2D());
	return true;
}

bool UWB_PlayerHitComponent::ReceiveDashHit(AActor* Dasher, FVector Direction)
{
	if (!CanBeHitBy(Dasher))
	{
		return false;
	}
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	// Down without a timer yet: the timer starts on landing (HandleLanded), or right away when the bump has no jump.
	if (!PlayerState->ChangePlayerState(WBTags::Player_State_Down))
	{
		return false;
	}

	RegisterHit(Dasher);

	// Sideways bump: perpendicular to the Dash, toward the side of the Dash line where this player stands.
	// The dasher runs through (BP_GenericCharacter ignores the victim), so a push along the Dash would put the victim back in its way.
	const FVector DashDirection = Direction.GetSafeNormal2D();
	const FVector ToVictim = (GetOwner()->GetActorLocation() - Dasher->GetActorLocation()).GetSafeNormal2D();
	FVector PushDirection = ToVictim;
	if (!DashDirection.IsNearlyZero())
	{
		const FVector DashRight = FVector::CrossProduct(FVector::UpVector, DashDirection);
		float Side = FVector::DotProduct(ToVictim, DashRight);
		if (FMath::IsNearlyZero(Side, 0.05f))
		{
			Side = FMath::RandBool() ? 1.f : -1.f; // Dead center: pick a side.
		}
		PushDirection = DashRight * FMath::Sign(Side);
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character)
	{
		// Face the push direction right away (the movement then keeps this orientation while sliding).
		Character->SetActorRotation(FRotator(0.f, PushDirection.Rotation().Yaw, 0.f));
		Character->LaunchCharacter(PushDirection * Profile->DownKnockbackStrength + FVector::UpVector * Profile->DownKnockbackUpStrength, true, true);
	}
	if (Character && Profile->DownKnockbackUpStrength > 0.f)
	{
		bAwaitingDownLanding = true;
	}
	else
	{
		PlayerState->ReturnToFreeAfter(Profile->DownDuration);
	}
	UE_LOG(LogWBPlayerHit, Verbose, TEXT("%s knocked down by %s."), *GetNameSafe(GetOwner()), *GetNameSafe(Dasher));
	OnHitReceived.Broadcast(Dasher, WBTags::Player_State_Down, PushDirection);
	return true;
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
	if (NewState == WBTags::Player_State_Down)
	{
		// Invulnerable (and blinking) from the knock down.
		StartGraceBlink();
	}
	if (OldState == WBTags::Player_State_Down)
	{
		bAwaitingDownLanding = false;
		if (const UWorld* World = GetWorld())
		{
			GraceEndTime = World->GetTimeSeconds() + GetActiveHitProfile()->PostDownGraceSeconds;
		}
		StartGraceBlink(); // Keeps blinking if already started (nothing restarts), until the end of the grace.
	}
}

void UWB_PlayerHitComponent::StartGraceBlink()
{
	const UWB_PlayerHitProfile* Profile = GetActiveHitProfile();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Profile->GraceBlinkMaterial || !Character || !Character->GetMesh() || !World || !IsInPostDownGrace())
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
	if (!Character || !Character->GetMesh() || !IsInPostDownGrace())
	{
		StopGraceBlink();
		return;
	}
	bGraceFlashOn = !bGraceFlashOn;
	Character->GetMesh()->SetOverlayMaterial(bGraceFlashOn ? GetActiveHitProfile()->GraceBlinkMaterial.Get() : OverlayBeforeBlink.Get());
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
		PlayerState->ReturnToFreeAfter(GetActiveHitProfile()->DownDuration);
	}
}
