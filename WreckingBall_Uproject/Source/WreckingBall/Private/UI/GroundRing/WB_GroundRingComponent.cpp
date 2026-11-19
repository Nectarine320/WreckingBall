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
	KickPercent = Percent;
	if (RingMaterial)
	{
		RingMaterial->SetScalarParameterValue(WBGroundRing::KickPercentParam, KickPercent);
	}
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
	SetComponentTickEnabled(bRingActive && bSnap);

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

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : Profile->GroundObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	if (!ObjectParams.IsValid())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WBGroundRingSnap), false, Owner);
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Profile->TraceLength);

	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams))
	{
		return false;
	}

	// The owner's X and Y, the ground's Z: the ring sits exactly under the owner.
	const float HeightOffset = RingMesh ? Profile->MeshHeightOffset : 0.f;
	RingVisual->SetWorldLocation(FVector(Start.X, Start.Y, Hit.ImpactPoint.Z + HeightOffset));

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
