#include "BlasterBolt.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Hazard.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float BoltSpeed = 4500.f;
	constexpr float BoltLife = 0.9f;
}

ABlasterBolt::ABlasterBolt()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(40.f);
	Light->SetAttenuationRadius(400.f);
	Light->SetLightColor(FLinearColor(1.f, 0.5f, 0.15f));
	Light->SetCastShadows(false);
}

void ABlasterBolt::Launch(const FVector& Direction, AActor* Shooter, UStaticMesh* InMesh, UMaterialInterface* Material)
{
	SetOwner(Shooter);
	Velocity = Direction.GetSafeNormal() * BoltSpeed;
	SetActorRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
	if (InMesh)
	{
		Mesh->SetStaticMesh(InMesh); // engine sphere: 100 cm; stretched along the flight direction
		SetActorScale3D(FVector(0.12f, 0.12f, 0.6f));
	}
	if (Material)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(4.f, 1.6f, 0.4f));
		Mesh->SetMaterial(0, MID);
	}
}

void ABlasterBolt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const FVector Start = GetActorLocation();
	const FVector End = Start + Velocity * DeltaSeconds;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Bolt), true, GetOwner());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(12.f), Params))
	{
		if (AHazard* Hazard = Cast<AHazard>(Hit.GetActor()))
		{
			if (Hazard->CanBeShot())
			{
				Hazard->Shatter();
			}
		}
		Destroy();
		return;
	}
	SetActorLocation(End);
	if (Age > BoltLife)
	{
		Destroy();
	}
}
