#include "Player/State/WB_PlayerStateComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
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
	RefreshTickEnabled();
}

const UWB_PlayerStateProfile* UWB_PlayerStateComponent::GetActiveProfile() const
{
	return PlayerStateProfile ? PlayerStateProfile.Get() : GetDefault<UWB_PlayerStateProfile>();
}

bool UWB_PlayerStateComponent::CanEnterState(FGameplayTag NewState) const
{
	if (!NewState.MatchesTag(WBTags::Player_State) || NewState == WBTags::Player_State || NewState == CurrentState)
	{
		return false;
	}

	const UWB_PlayerStateProfile* Profile = GetActiveProfile();
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
	return !GetActiveProfile()->StatesBlockingMovement.HasTagExact(CurrentState);
}

bool UWB_PlayerStateComponent::CanSprint() const
{
	return !GetActiveProfile()->StatesBlockingSprint.HasTagExact(CurrentState);
}

float UWB_PlayerStateComponent::GetMoveSpeedMultiplier() const
{
	if (!CanMove())
	{
		return 0.f;
	}
	if (IsInChargedAction())
	{
		return FMath::Lerp(1.f, GetActiveProfile()->ActionMoveSpeedMultiplier, ActionCharge);
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

	FVector RunDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();
	if (!CelebrationSteering.IsNearlyZero())
	{
		// Turn the running direction toward the stick, at most CelebrationTurnRate degrees per second.
		const float MaxStepRadians = FMath::DegreesToRadians(GetActiveProfile()->CelebrationTurnRate) * DeltaTime;
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
		const FString Text = FString::Printf(TEXT("%s | %s"), *ShortTagName(CurrentState), *ShortTagName(CurrentHolding));
		DrawDebugString(GetWorld(), Owner->GetActorLocation() + FVector(0.f, 0.f, DebugTextHeight), Text,
			nullptr, FColor::Yellow, 0.f, true, 1.2f);
	}
#endif
}
