#include "Player/State/WB_PlayerStateComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Player/Energy/WB_PlayerEnergyProfile.h"
#include "Player/Hit/WB_PlayerHitComponent.h"
#include "Player/State/WB_PlayerStateProfile.h"
#include "Player/State/WB_PlayerStateTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBPlayerState, Log, All);

namespace
{
	/** Last segment of a tag for the debug text ("Player.State.Free" -> "Free"). */
	FString ShortTagName(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), DotIndex))
		{
			Name.RightChopInline(DotIndex + 1);
		}
		return Name;
	}
}

UWB_PlayerStateComponent::UWB_PlayerStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Ticks only to draw the debug text.

	CurrentState = WBTags::Player_State_Free;
	CurrentHolding = WBTags::Player_Holding_None;
}

void UWB_PlayerStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Default walk speed (set by the construction script): base of the celebration ramp, restored after it.
	// Ticking before the movement makes the ramped speed apply on the same frame.
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			DefaultWalkSpeed = Movement->MaxWalkSpeed;
			Movement->AddTickPrerequisiteComponent(this);
		}
	}
	RefreshTickEnabled();
}

void UWB_PlayerStateComponent::ApplyWalkSpeedMultiplier(float Multiplier) const
{
	if (DefaultWalkSpeed <= 0.f)
	{
		return;
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = DefaultWalkSpeed * Multiplier;
		}
	}
}

const UWB_PlayerStateProfile* UWB_PlayerStateComponent::GetActiveProfile() const
{
	if (!PlayerStateProfile && !bWarnedNoStateProfile)
	{
		bWarnedNoStateProfile = true;
		UE_LOG(LogWBPlayerState, Warning, TEXT("%s: no PlayerStateProfile on %s (DA_PlayerState_Default expected). Every state change is refused."), *GetNameSafe(GetOwner()), *GetName());
	}
	return PlayerStateProfile.Get();
}

const UWB_PlayerEnergyProfile* UWB_PlayerStateComponent::GetActiveEnergyProfile() const
{
	if (!EnergyProfile && !bWarnedNoEnergyProfile)
	{
		bWarnedNoEnergyProfile = true;
		UE_LOG(LogWBPlayerState, Warning, TEXT("%s: no EnergyProfile on %s (DA_PlayerEnergy_Default expected). Sprint and energy costs read None."), *GetNameSafe(GetOwner()), *GetName());
	}
	return EnergyProfile.Get();
}

bool UWB_PlayerStateComponent::CanEnterState(FGameplayTag NewState) const
{
	if (!NewState.MatchesTag(WBTags::Player_State) || NewState == WBTags::Player_State || NewState == CurrentState)
	{
		return false;
	}

	const UWB_PlayerStateProfile* Profile = GetActiveProfile();
	if (!Profile)
	{
		return false;
	}
	const FGameplayTagContainer* Allowed = Profile->AllowedTransitions.Find(CurrentState);
	if (!Allowed || !Allowed->HasTagExact(NewState))
	{
		return false;
	}

	const bool bHolding = IsHoldingAnything();
	if (bHolding && Profile->StatesRequiringEmptyHands.HasTagExact(NewState))
	{
		return false;
	}
	if (!bHolding && Profile->StatesRequiringHolding.HasTagExact(NewState))
	{
		return false;
	}
	return true;
}

bool UWB_PlayerStateComponent::ChangePlayerState(FGameplayTag NewState)
{
	if (!CanEnterState(NewState))
	{
		UE_LOG(LogWBPlayerState, Verbose, TEXT("%s: %s -> %s refused (holding %s)."),
			*GetNameSafe(GetOwner()), *CurrentState.ToString(), *NewState.ToString(), *CurrentHolding.ToString());
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StateDurationTimer);
	}

	const FGameplayTag OldState = CurrentState;
	CurrentState = NewState;
	if (!IsInChargedAction())
	{
		ActionCharge = 0.f;
	}
	CelebrationSteering = FVector::ZeroVector;
	if (NewState == WBTags::Player_State_Celebrating)
	{
		const UWorld* CurrentWorld = GetWorld();
		CelebrationStartTime = CurrentWorld ? CurrentWorld->GetTimeSeconds() : 0.0;
		ApplyWalkSpeedMultiplier(GetActiveProfile()->CelebrationStartSpeedMultiplier); // Profile checked by CanEnterState.
	}
	else if (OldState == WBTags::Player_State_Celebrating)
	{
		ApplyWalkSpeedMultiplier(1.f); // Back to the default walk speed.
	}
	RefreshTickEnabled();
	OnPlayerStateChanged.Broadcast(OldState, NewState);
	return true;
}

bool UWB_PlayerStateComponent::TryEnterState(FGameplayTag NewState)
{
	if (ChangePlayerState(NewState))
	{
		return true;
	}
	OnActionRefused.Broadcast(NewState);
	return false;
}

bool UWB_PlayerStateComponent::ChangePlayerStateForDuration(FGameplayTag NewState, float Duration)
{
	if (!ChangePlayerState(NewState))
	{
		return false;
	}
	ReturnToFreeAfter(Duration);
	return true;
}

void UWB_PlayerStateComponent::ReturnToFreeAfter(float Duration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FGameplayTag StateToEnd = CurrentState;
	if (Duration <= 0.f)
	{
		ChangePlayerState(WBTags::Player_State_Free);
		return;
	}

	TWeakObjectPtr<UWB_PlayerStateComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(StateDurationTimer, FTimerDelegate::CreateLambda([WeakThis, StateToEnd]()
	{
		if (WeakThis.IsValid() && WeakThis->CurrentState == StateToEnd)
		{
			WeakThis->ChangePlayerState(WBTags::Player_State_Free);
		}
	}), Duration, false);
}

bool UWB_PlayerStateComponent::CanMove() const
{
	// No profile: the move stays free (only the states and the sprint are cut), so the missing asset is obvious but the game still runs.
	const UWB_PlayerStateProfile* Profile = GetActiveProfile();
	return !Profile || !Profile->StatesBlockingMovement.HasTagExact(CurrentState);
}

bool UWB_PlayerStateComponent::CanSprint() const
{
	const UWB_PlayerStateProfile* Profile = GetActiveProfile();
	return Profile && !Profile->StatesBlockingSprint.HasTagExact(CurrentState);
}

float UWB_PlayerStateComponent::GetMoveSpeedMultiplier() const
{
	if (!CanMove())
	{
		return 0.f;
	}
	// Possession penalty (lot 11.2), combined with the other multipliers.
	const UWB_PlayerStateProfile* HoldingProfile = GetActiveProfile();
	const float Holding = HoldingProfile && CurrentHolding.MatchesTag(WBTags::Player_Holding_Ball) ? HoldingProfile->HoldingBallMoveSpeedMultiplier : 1.f;
	return Holding * GetStateMoveSpeedMultiplier();
}

float UWB_PlayerStateComponent::GetStateMoveSpeedMultiplier() const
{
	if (IsInChargedAction())
	{
		// Full kick charge: no more forward motion, but the player can still turn (user request 2026-11-16).
		// A tiny scale keeps the input direction, so bOrientRotationToMovement turns the player toward the stick
		// while the speed stays below 1 cm/s. A strict 0 would also stop the rotation.
		if (CurrentState == WBTags::Player_State_Kicking && ActionCharge >= 1.f)
		{
			return 0.001f;
		}
		const UWB_PlayerStateProfile* Profile = GetActiveProfile();
		return Profile ? FMath::Lerp(1.f, Profile->ActionMoveSpeedMultiplier, ActionCharge) : 1.f;
	}
	return 1.f;
}

void UWB_PlayerStateComponent::SetActionCharge(float Charge)
{
	if (IsInChargedAction())
	{
		ActionCharge = FMath::Clamp(Charge, 0.f, 1.f);
	}
}

void UWB_PlayerStateComponent::SetCelebrationSteering(FVector WorldDirection)
{
	CelebrationSteering = WorldDirection.GetSafeNormal2D();
}

void UWB_PlayerStateComponent::RefreshTickEnabled()
{
	SetComponentTickEnabled(bDebugShowState || CurrentState == WBTags::Player_State_Celebrating);
}

void UWB_PlayerStateComponent::TickCelebrationRun(float DeltaTime)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	// Speed curve (user request 2026-11-16): explosive rise from the start to the peak at mid-celebration (ease-out, CelebrationRiseExponent),
	// then a gradual fade back to the default walk speed at the end (smoothstep), so the end of the state has no speed jump.
	const UWB_PlayerStateProfile* Profile = GetActiveProfile();
	if (!Profile)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Elapsed = World ? static_cast<float>(World->GetTimeSeconds() - CelebrationStartTime) : 0.f;
	const float Alpha = Profile->CelebrationDuration > 0.f ? FMath::Clamp(Elapsed / Profile->CelebrationDuration, 0.f, 1.f) : 1.f;
	float SpeedMultiplier;
	if (Alpha <= 0.5f)
	{
		const float Rise = Alpha / 0.5f;
		SpeedMultiplier = FMath::Lerp(Profile->CelebrationStartSpeedMultiplier, Profile->CelebrationEndSpeedMultiplier, 1.f - FMath::Pow(1.f - Rise, Profile->CelebrationRiseExponent));
	}
	else
	{
		const float Fade = (Alpha - 0.5f) / 0.5f;
		SpeedMultiplier = FMath::Lerp(Profile->CelebrationEndSpeedMultiplier, 1.f, FMath::SmoothStep(0.f, 1.f, Fade));
	}
	ApplyWalkSpeedMultiplier(SpeedMultiplier);

	FVector RunDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();
	if (!CelebrationSteering.IsNearlyZero())
	{
		// Turn the running direction toward the stick, at most CelebrationTurnRate degrees per second.
		const float MaxStepRadians = FMath::DegreesToRadians(Profile->CelebrationTurnRate) * DeltaTime;
		const float AngleToTarget = FMath::Acos(FMath::Clamp(FVector::DotProduct(RunDirection, CelebrationSteering), -1.f, 1.f));
		if (AngleToTarget <= MaxStepRadians)
		{
			RunDirection = CelebrationSteering;
		}
		else
		{
			const float Sign = FVector::CrossProduct(RunDirection, CelebrationSteering).Z >= 0.f ? 1.f : -1.f;
			RunDirection = RunDirection.RotateAngleAxisRad(Sign * MaxStepRadians, FVector::UpVector);
		}
	}
	Pawn->AddMovementInput(RunDirection, 1.f);
}

bool UWB_PlayerStateComponent::IsInChargedAction() const
{
	return CurrentState == WBTags::Player_State_Kicking || CurrentState == WBTags::Player_State_Throwing;
}

bool UWB_PlayerStateComponent::ChangeHolding(FGameplayTag NewHolding)
{
	if (!NewHolding.MatchesTag(WBTags::Player_Holding) || NewHolding == WBTags::Player_Holding)
	{
		UE_LOG(LogWBPlayerState, Warning, TEXT("%s: ChangeHolding needs a Player.Holding.* tag, got '%s'."),
			*GetNameSafe(GetOwner()), *NewHolding.ToString());
		return false;
	}
	if (NewHolding == CurrentHolding)
	{
		return false;
	}

	const FGameplayTag OldHolding = CurrentHolding;
	CurrentHolding = NewHolding;
	OnHoldingChanged.Broadcast(OldHolding, NewHolding);
	return true;
}

bool UWB_PlayerStateComponent::IsInState(FGameplayTag State) const
{
	return State.IsValid() && CurrentState.MatchesTag(State);
}

bool UWB_PlayerStateComponent::IsHoldingAnything() const
{
	return CurrentHolding.IsValid() && CurrentHolding != WBTags::Player_Holding_None;
}

void UWB_PlayerStateComponent::SetDebugShowState(bool bShow)
{
	bDebugShowState = bShow;
	RefreshTickEnabled();
}

void UWB_PlayerStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentState == WBTags::Player_State_Celebrating)
	{
		TickCelebrationRun(DeltaTime);
	}

#if ENABLE_DRAW_DEBUG
	const AActor* Owner = GetOwner();
	if (bDebugShowState && Owner)
	{
		FString Text = FString::Printf(TEXT("%s | %s"), *ShortTagName(CurrentState), *ShortTagName(CurrentHolding));
		FColor Color = FColor::Yellow;
		// Invulnerability (Down + post-Down grace) explains the blink: shown in cyan.
		const UWB_PlayerHitComponent* Hit = Owner->FindComponentByClass<UWB_PlayerHitComponent>();
		if (Hit && Hit->IsInPostDownGrace())
		{
			Text += TEXT(" | Invulnerable");
			Color = FColor::Cyan;
		}
		DrawDebugString(GetWorld(), Owner->GetActorLocation() + FVector(0.f, 0.f, DebugTextHeight), Text,
			nullptr, Color, 0.f, true, 1.2f);
	}
#endif
}
