#include "UI/GroundRing/WB_GroundRingComponent.h"

#include "CollisionQueryParams.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/GroundRing/WB_GroundRingProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBGroundRing, Log, All);

namespace WBGroundRing
{
	// Parameter names of M_GroundRing.
	static const FName RingColorParam(TEXT("RingColor"));
	static const FName KickPercentParam(TEXT("KickPercent"));
	static const FName ShadowStrengthParam(TEXT("ShadowStrength"));
	static const FName GaugeThicknessParam(TEXT("GaugeThickness"));
	static const FName KickColorStartParam(TEXT("KickColorStart"));
	static const FName KickColorEndParam(TEXT("KickColorEnd"));
}

UWB_GroundRingComponent::UWB_GroundRingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After physics: a snapped ring follows the final location of a simulated ball.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

bool UWB_GroundRingComponent::UsesSnap() const
{
	return Profile && Profile->bSnapToGround;
}

void UWB_GroundRingComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Profile || !Owner)
	{
		UE_LOG(LogWBGroundRing, Warning, TEXT("%s: GroundRing needs a Profile. No ring created."), *GetPathNameSafe(this));
		return;
	}

	const bool bCreated = Profile->bUseMesh ? CreateMesh(Owner) : CreateDecal(Owner);
	if (!bCreated || !RingMaterial)
	{
		return;
	}

	RingMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(WBGroundRing::ShadowStrengthParam), BaseShadowStrength);
	RingMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(WBGroundRing::GaugeThicknessParam), BaseGaugeThickness);
	RingMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(WBGroundRing::KickColorStartParam), BaseKickColorStart);
	RingMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(WBGroundRing::KickColorEndParam), BaseKickColorEnd);
	if (bHasRingColor)
	{
		RingMaterial->SetVectorParameterValue(WBGroundRing::RingColorParam, RingColor);
	}
	RingMaterial->SetScalarParameterValue(WBGroundRing::KickPercentParam, KickPercent);

	bRingActive = Profile->bStartActive;
	RefreshState();
}

bool UWB_GroundRingComponent::CreateDecal(AActor* Owner)
{
	if (!Profile->DecalMaterial)
	{
		UE_LOG(LogWBGroundRing, Warning, TEXT("%s: GroundRing profile %s has no DecalMaterial. No ring created."), *GetPathNameSafe(this), *GetNameSafe(Profile));
		return false;
	}

	Decal = NewObject<UDecalComponent>(Owner, MakeUniqueObjectName(Owner, UDecalComponent::StaticClass(), TEXT("GroundRingDecal")));
	Decal->SetupAttachment(this);
	// Decals project along their X axis: pitch -90 points it down. Absolute so the owner's turns and rolls never tilt it.
	Decal->SetUsingAbsoluteRotation(true);
	Decal->SetUsingAbsoluteLocation(UsesSnap());
	Decal->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
	Decal->DecalSize = FVector(Profile->ProjectionHalfHeight, Profile->Radius, Profile->Radius);
	Decal->SortOrder = Profile->SortOrder;
	// Seen from a high camera, the ring is small on screen: never fade it out.
	Decal->SetFadeScreenSize(0.f);
	Decal->SetDecalMaterial(Profile->DecalMaterial);
	Decal->RegisterComponent();

	RingVisual = Decal;
	RingMaterial = Decal->CreateDynamicMaterialInstance();
	return true;
}

bool UWB_GroundRingComponent::CreateMesh(AActor* Owner)
{
	if (!Profile->RingMesh || !Profile->MeshMaterial)
	{
		UE_LOG(LogWBGroundRing, Warning, TEXT("%s: GroundRing profile %s is in mesh mode but has no RingMesh or MeshMaterial. No ring created."),
			*GetPathNameSafe(this), *GetNameSafe(Profile));
		return false;
	}

	RingMesh = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("GroundRingMesh")));
	RingMesh->SetupAttachment(this);
	RingMesh->SetStaticMesh(Profile->RingMesh);
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetGenerateOverlapEvents(false);
	RingMesh->SetCastShadow(false);
	RingMesh->bReceivesDecals = false;
	RingMesh->SetCanEverAffectNavigation(false);
	RingMesh->SetTranslucentSortPriority(Profile->MeshSortPriority);
	// Absolute rotation: flat on the ground whatever the owner's turns and rolls.
	RingMesh->SetUsingAbsoluteRotation(true);
	RingMesh->SetUsingAbsoluteLocation(UsesSnap());
	RingMesh->SetWorldRotation(FRotator::ZeroRotator);
	RingMesh->SetRelativeLocation(FVector(0.f, 0.f, Profile->MeshHeightOffset));

	// Width of the mesh matches 2 x Radius, whatever the size of the source mesh.
	const FVector Extent = Profile->RingMesh->GetBounds().BoxExtent;
	const float HalfWidth = FMath::Max3(static_cast<float>(Extent.X), static_cast<float>(Extent.Y), 1.f);
	const float Scale = Profile->Radius / HalfWidth;
	RingMesh->SetWorldScale3D(FVector(Scale, Scale, 1.f));

	RingMesh->SetMaterial(0, Profile->MeshMaterial);
	RingMesh->RegisterComponent();

	RingVisual = RingMesh;
	RingMaterial = RingMesh->CreateDynamicMaterialInstance(0, Profile->MeshMaterial);
	return true;
}

void UWB_GroundRingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Decal)
	{
		Decal->DestroyComponent();
		Decal = nullptr;
	}
	if (RingMesh)
	{
		RingMesh->DestroyComponent();
		RingMesh = nullptr;
	}
	RingVisual = nullptr;
	RingMaterial = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UWB_GroundRingComponent::SetRingColor(FLinearColor Color)
{
	RingColor = Color;
	bHasRingColor = true;
	if (RingMaterial)
	{
		RingMaterial->SetVectorParameterValue(WBGroundRing::RingColorParam, RingColor);
	}
}

void UWB_GroundRingComponent::SetKickPercent(float Percent)
{
	Percent = FMath::Clamp(Percent, 0.f, 1.f);
	if (Percent == KickPercent)
	{
		return;
	}
	const bool bReachedFull = Percent >= 1.f && KickPercent < 1.f;
	KickPercent = Percent;
	if (RingMaterial)
	{
		RingMaterial->SetScalarParameterValue(WBGroundRing::KickPercentParam, KickPercent);
	}

	if (bReachedFull && RingMaterial && Profile && Profile->bUseFullChargePulse)
	{
		// The gauge just got full: one pulse.
		bPulsing = true;
		PulseElapsed = 0.f;
		SetComponentTickEnabled(true);
	}
	else if (KickPercent <= 0.f && bPulsing)
	{
		// Charge released or cancelled during the pulse: back to the base look at once.
		bPulsing = false;
		ApplyPulse(0.f);
		RefreshState();
	}
}

void UWB_GroundRingComponent::ApplyPulse(float Weight)
{
	if (!RingMaterial || !Profile)
	{
		return;
	}
	RingMaterial->SetScalarParameterValue(WBGroundRing::GaugeThicknessParam, FMath::Lerp(BaseGaugeThickness, Profile->FullChargePulseThickness, Weight));
	RingMaterial->SetVectorParameterValue(WBGroundRing::KickColorStartParam, FMath::Lerp(BaseKickColorStart, Profile->FullChargePulseColor, Weight));
	RingMaterial->SetVectorParameterValue(WBGroundRing::KickColorEndParam, FMath::Lerp(BaseKickColorEnd, Profile->FullChargePulseColor, Weight));
}

void UWB_GroundRingComponent::SetRingActive(bool bActive)
{
	if (bActive == bRingActive)
	{
		return;
	}
	bRingActive = bActive;
	RefreshState();
}

void UWB_GroundRingComponent::RefreshState()
{
	if (!RingVisual)
	{
		return;
	}
	const bool bSnap = UsesSnap();
	SetComponentTickEnabled((bRingActive && bSnap) || bPulsing);

	bool bShowDecal = bRingActive;
	if (bShowDecal && bSnap)
	{
		bShowDecal = UpdateSnap();
	}
	RingVisual->SetVisibility(bShowDecal);
}

void UWB_GroundRingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bPulsing && Profile)
	{
		// Up then down: thicker and colored at mid-pulse, back to the base look at the end.
		PulseElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(PulseElapsed / FMath::Max(Profile->FullChargePulseDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		ApplyPulse(FMath::Sin(Alpha * UE_PI));
		if (Alpha >= 1.f)
		{
			bPulsing = false;
			ApplyPulse(0.f);
			RefreshState();
		}
	}

	if (RingVisual && bRingActive && UsesSnap())
	{
		const bool bOnGround = UpdateSnap();
		if (RingVisual->IsVisible() != bOnGround)
		{
			RingVisual->SetVisibility(bOnGround);
		}
	}
}

bool UWB_GroundRingComponent::UpdateSnap()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !RingVisual || !Profile)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WBGroundRingSnap), false, Owner);
	// From the center of the owner's root bounds, not the actor location: a mesh whose pivot is off center (the soccer ball's is at its base)
	// would swing the ring around the owner while it rolls.
	const USceneComponent* Root = Owner->GetRootComponent();
	const FVector Start = Root ? Root->Bounds.Origin : Owner->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Profile->TraceLength);

	FHitResult Hit;
	if (Profile->bTraceByChannel)
	{
		const ECollisionChannel Channel = UEngineTypes::ConvertToCollisionChannel(Profile->GroundTraceChannel);
		if (!World->LineTraceSingleByChannel(Hit, Start, End, Channel, QueryParams))
		{
			return false;
		}
	}
	else
	{
		FCollisionObjectQueryParams ObjectParams;
		for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : Profile->GroundObjectTypes)
		{
			ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
		}
		if (!ObjectParams.IsValid() || !World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams))
		{
			return false;
		}
	}

	// The owner's X and Y, the ground's Z: the ring sits exactly under the owner, flat (rotation reapplied so a rolling owner never tilts it).
	const float HeightOffset = RingMesh ? Profile->MeshHeightOffset : 0.f;
	const FRotator FlatRotation = RingMesh ? FRotator::ZeroRotator : FRotator(-90.f, 0.f, 0.f);
	RingVisual->SetWorldLocationAndRotation(FVector(Start.X, Start.Y, Hit.ImpactPoint.Z + HeightOffset), FlatRotation);

	if (Profile->bFadeShadowWithHeight && RingMaterial)
	{
		const float Height = Start.Z - Hit.ImpactPoint.Z;
		const float Factor = FMath::GetMappedRangeValueClamped(
			FVector2f(Profile->FadeStartHeight, FMath::Max(Profile->FadeEndHeight, Profile->FadeStartHeight + 1.f)),
			FVector2f(1.f, Profile->MinShadowFactor), Height);
		RingMaterial->SetScalarParameterValue(WBGroundRing::ShadowStrengthParam, BaseShadowStrength * Factor);
	}
	return true;
}
