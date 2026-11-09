#include "PowerUp.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

APowerUp::APowerUp()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(1.3f));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(20.f);
	Light->SetAttenuationRadius(450.f);
	Light->SetCastShadows(false);
}

FLinearColor APowerUp::KindColor(EPowerUpKind K)
{
	switch (K)
	{
	case EPowerUpKind::Heal: return FLinearColor(0.3f, 1.f, 0.4f);
	case EPowerUpKind::Shield: return FLinearColor(0.3f, 0.6f, 1.f);
	default: return FLinearColor(1.f, 0.5f, 0.15f);
	}
}

const TCHAR* APowerUp::KindName(EPowerUpKind K)
{
	switch (K)
	{
	case EPowerUpKind::Heal: return TEXT("Repair kit");
	case EPowerUpKind::Shield: return TEXT("Shield");
	default: return TEXT("Blaster");
	}
}

void APowerUp::Configure(EPowerUpKind InKind, UStaticMesh* InMesh)
{
	Kind = InKind;
	Mesh->SetStaticMesh(InMesh);
	Light->SetLightColor(KindColor(Kind));
	Clock = FMath::FRandRange(0.f, 5.f);
}

void APowerUp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, FMath::Sin(Clock * 2.5f) * 10.f), FRotator(0.f, Clock * 90.f, 0.f));
}
