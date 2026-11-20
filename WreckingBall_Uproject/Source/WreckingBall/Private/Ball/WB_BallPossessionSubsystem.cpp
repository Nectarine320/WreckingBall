#include "Ball/WB_BallPossessionSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

namespace
{
	/** Seconds after the kick before the speed test may end the kicked mode: the impulse is applied at the next physics step. Technical value. */
	constexpr double MinKickedTime = 0.1;

	/** Niagara user parameter switched by the trail logic (NS_BallTrail). */
	const FName TrailActiveParameter(TEXT("TrailActive"));
}

void UWB_BallPossessionSubsystem::RegisterBallTrail(const UObject* WorldContextObject, AActor* Ball, UNiagaraComponent* Trail, float StartSpeed, float StopSpeed, float ImpactSpeed)
{
	UWB_BallPossessionSubsystem* Self = Get(WorldContextObject);
	if (!Self || !Ball || !Trail)
	{
		return;
	}
	FWB_BallTrailRecord& Record = Self->Trails.FindOrAdd(Ball);
	Record.Trail = Trail;
	Record.StartSpeed = StartSpeed;
	Record.StopSpeed = StopSpeed;
	Record.ImpactSpeed = ImpactSpeed;
	Record.bOn = false;
	Record.bHasLastVelocity = false;
	Trail->SetVariableFloat(TrailActiveParameter, 0.f);
}

void UWB_BallPossessionSubsystem::TickTrails()
{
	for (auto It = Trails.CreateIterator(); It; ++It)
	{
		const AActor* Ball = It.Key().Get();
		FWB_BallTrailRecord& Record = It.Value();
		UNiagaraComponent* Trail = Record.Trail.Get();
		if (!Ball || !Trail)
		{
			It.RemoveCurrent();
			continue;
		}
		const UPrimitiveComponent* Body = FindBallBody(Ball);
		const FVector Velocity = Body ? Body->GetComponentVelocity() : Ball->GetVelocity();
		const float Speed = Velocity.Size();
		const float Impact = Record.bHasLastVelocity ? FVector::Dist(Velocity, Record.LastVelocity) : 0.f;
		Record.LastVelocity = Velocity;
		Record.bHasLastVelocity = true;

		bool bOn = Record.bOn;
		if (IsBallHeld(this, Ball))
		{
			bOn = false;
		}
		else
		{
			if (!bOn && (Speed > Record.StartSpeed || Impact >= Record.ImpactSpeed))
			{
				bOn = true;
			}
			if (bOn && Speed < Record.StopSpeed)
			{
				bOn = false;
			}
		}
		if (bOn != Record.bOn)
		{
			Record.bOn = bOn;
			Trail->SetVariableFloat(TrailActiveParameter, bOn ? 1.f : 0.f);
		}
	}
}

UWB_BallPossessionSubsystem* UWB_BallPossessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWB_BallPossessionSubsystem>() : nullptr;
}

UPrimitiveComponent* UWB_BallPossessionSubsystem::FindBallBody(const AActor* Ball)
{
	if (!Ball)
	{
		return nullptr;
	}
	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Ball->GetRootComponent());
	if (Root && Root->IsSimulatingPhysics())
	{
		return Root;
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Ball);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsSimulatingPhysics())
		{
			return Primitive;
		}
	}
	return Root;
}

void UWB_BallPossessionSubsystem::MarkBallKicked(const UObject* WorldContextObject, AActor* Ball, AActor* Kicker, float KickedReleaseSpeed)
{
	UWB_BallPossessionSubsystem* Self = Get(WorldContextObject);
	if (!Self || !Ball)
	{
		return;
	}
	FWB_BallKickRecord& Record = Self->Records.FindOrAdd(Ball);
	Record.Kicker = Kicker;
	Record.KickTime = Self->GetWorld()->GetTimeSeconds();
	Record.ReleaseSpeed = KickedReleaseSpeed;
	Record.bKicked = KickedReleaseSpeed > 0.f;
}

void UWB_BallPossessionSubsystem::ClearBallKick(const UObject* WorldContextObject, AActor* Ball)
{
	if (UWB_BallPossessionSubsystem* Self = Get(WorldContextObject))
	{
		Self->Records.Remove(Ball);
	}
}

bool UWB_BallPossessionSubsystem::IsBallKicked(const UObject* WorldContextObject, const AActor* Ball)
{
	const UWB_BallPossessionSubsystem* Self = Get(WorldContextObject);
	const FWB_BallKickRecord* Record = Self ? Self->Records.Find(Ball) : nullptr;
	return Record && Record->bKicked;
}

bool UWB_BallPossessionSubsystem::IsBallHeld(const UObject* WorldContextObject, const AActor* Ball)
{
	return GetBallPossessor(WorldContextObject, Ball) != nullptr;
}

AActor* UWB_BallPossessionSubsystem::GetBallPossessor(const UObject* WorldContextObject, const AActor* Ball)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !Ball)
	{
		return nullptr;
	}
	if (const UWB_BallPossessionSubsystem* Self = World->GetSubsystem<UWB_BallPossessionSubsystem>())
	{
		if (const TWeakObjectPtr<AActor>* Dribbler = Self->Dribblers.Find(Ball))
		{
			if (AActor* Player = Dribbler->Get())
			{
				return Player;
			}
		}
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		TInlineComponentArray<UPhysicsHandleComponent*> Handles(*It);
		for (const UPhysicsHandleComponent* Handle : Handles)
		{
			const UPrimitiveComponent* Grabbed = Handle ? Handle->GetGrabbedComponent() : nullptr;
			if (Grabbed && Grabbed->GetOwner() == Ball)
			{
				return *It;
			}
		}
	}
	return nullptr;
}

void UWB_BallPossessionSubsystem::SetBallDribbler(const UObject* WorldContextObject, AActor* Ball, AActor* Player)
{
	UWB_BallPossessionSubsystem* Self = Get(WorldContextObject);
	if (Self && Ball && Player)
	{
		const TWeakObjectPtr<AActor>* Previous = Self->Dribblers.Find(Ball);
		AActor* OldPossessor = Previous ? Previous->Get() : nullptr;
		Self->Dribblers.Add(Ball, Player);
		if (OldPossessor != Player)
		{
			Self->OnBallPossessionChanged.Broadcast(Ball, Player, OldPossessor);
		}
	}
}

void UWB_BallPossessionSubsystem::ClearBallDribbler(const UObject* WorldContextObject, const AActor* Ball, const AActor* Player)
{
	UWB_BallPossessionSubsystem* Self = Get(WorldContextObject);
	const TWeakObjectPtr<AActor>* Dribbler = Self ? Self->Dribblers.Find(Ball) : nullptr;
	if (Dribbler && (!Dribbler->IsValid() || Dribbler->Get() == Player))
	{
		AActor* OldPossessor = Dribbler->Get();
		Self->Dribblers.Remove(Ball);
		if (OldPossessor)
		{
			Self->OnBallPossessionChanged.Broadcast(const_cast<AActor*>(Ball), nullptr, OldPossessor);
		}
	}
}

bool UWB_BallPossessionSubsystem::CanBeCaughtByWalking(const AActor* Ball, const AActor* Player, float KickerRecatchDelay) const
{
	if (!Ball || IsBallHeld(this, Ball))
	{
		return false;
	}
	const FWB_BallKickRecord* Record = Records.Find(Ball);
	if (!Record)
	{
		return true;
	}
	if (Record->bKicked)
	{
		return false;
	}
	const bool bOwnKick = Record->Kicker.Get() == Player;
	return !bOwnKick || GetWorld()->GetTimeSeconds() - Record->KickTime >= KickerRecatchDelay;
}

void UWB_BallPossessionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickTrails();

	for (auto It = Dribblers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		const AActor* Ball = It.Key().Get();
		if (!Ball)
		{
			It.RemoveCurrent();
			continue;
		}
		FWB_BallKickRecord& Record = It.Value();
		if (!Record.bKicked || Now - Record.KickTime < MinKickedTime)
		{
			continue;
		}
		const UPrimitiveComponent* Body = FindBallBody(Ball);
		const float Speed = Body ? Body->GetComponentVelocity().Size() : Ball->GetVelocity().Size();
		if (Speed < Record.ReleaseSpeed)
		{
			// Slow enough: anyone may catch it again (the kicker keeps its delay, the record stays for it).
			Record.bKicked = false;
		}
	}
}

TStatId UWB_BallPossessionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWB_BallPossessionSubsystem, STATGROUP_Tickables);
}
