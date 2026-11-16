#include "Player/State/WB_PlayerStateComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
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
	SetComponentTickEnabled(bDebugShowState);
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

	const FGameplayTag OldState = CurrentState;
	CurrentState = NewState;
	OnPlayerStateChanged.Broadcast(OldState, NewState);
	return true;
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
	SetComponentTickEnabled(bShow);
}

void UWB_PlayerStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
