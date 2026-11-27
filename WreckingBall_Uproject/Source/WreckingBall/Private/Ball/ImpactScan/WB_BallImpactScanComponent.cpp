#include "Ball/ImpactScan/WB_BallImpactScanComponent.h"

#include "Ball/ImpactScan/WB_BallImpactScanProfile.h"
#include "Ball/WB_BallPossessionSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBBallScan, Log, All);

UWB_BallImpactScanComponent::UWB_BallImpactScanComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWB_BallImpactScanComponent::StartScan()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!Profile)
	{
		// No silent fallback: without its profile the ball does not scan.
		if (!bWarnedNoProfile)
		{
			bWarnedNoProfile = true;
			UE_LOG(LogWBBallScan, Warning, TEXT("%s: no Profile on %s (DA_BallImpactScan_Default expected). The kicked ball does not scan for impacts."), *GetNameSafe(GetOwner()), *GetName());
		}
		return;
	}
	if (World->GetTimerManager().IsTimerActive(ScanTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(ScanTimer, this, &UWB_BallImpactScanComponent::RunScan, Profile->ScanInterval, true, 0.f);
}

void UWB_BallImpactScanComponent::StopScan()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimer);
	}
}

bool UWB_BallImpactScanComponent::IsScanning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(ScanTimer);
}

float UWB_BallImpactScanComponent::GetMinSpeed() const
{
	return Profile ? Profile->MinSpeed : 0.f;
}

void UWB_BallImpactScanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScan();
	Super::EndPlay(EndPlayReason);
}

void UWB_BallImpactScanComponent::RunScan()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Profile)
	{
		StopScan();
		return;
	}

	const UPrimitiveComponent* Body = UWB_BallPossessionSubsystem::FindBallBody(Owner);
	const FVector Velocity = Body ? Body->GetComponentVelocity() : Owner->GetVelocity();
	if (Velocity.SizeSquared() <= FMath::Square(Profile->MinSpeed))
	{
		StopScan();
		OnScanStopped.Broadcast();
		return;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Velocity * Profile->LookAheadTime;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WBBallImpactScan), false, Owner);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Profile->TraceChannel, FCollisionShape::MakeSphere(Profile->ScanRadius), Params);

#if ENABLE_DRAW_DEBUG
	if (Profile->bDrawDebug)
	{
		DrawDebugLine(World, Start, End, FColor::Green, false, Profile->ScanInterval);
		for (const FHitResult& Hit : Hits)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, Profile->ScanRadius, 8, Hit.bBlockingHit ? FColor::Red : FColor::Yellow, false, Profile->ScanInterval);
		}
	}
#endif

	for (const FHitResult& Hit : Hits)
	{
		if (Hit.bBlockingHit)
		{
			OnScanHit.Broadcast(Hit);
		}
	}
}
