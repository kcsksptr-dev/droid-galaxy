#include "ShipPart.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MissionTypes.h"

AShipPart::AShipPart()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Part"));
	PartMesh->SetupAttachment(Root);
	PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PartMesh->SetRelativeScale3D(FVector(1.6f));

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Root);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);
	// engine cylinder is 100 x 100 cm centred: make a 16 cm wide, 30 m tall column standing on the part
	Beacon->SetRelativeScale3D(FVector(0.16f, 0.16f, 30.f));
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 1500.f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(30.f);
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
}

void AShipPart::Init(int32 InPartIndex, UStaticMesh* Mesh, UStaticMesh* BeaconMesh, UMaterialInterface* BeaconMaterial)
{
	PartIndex = InPartIndex;
	Clock = PartIndex * 1.3f;
	const FLinearColor Color = Mission::Part(PartIndex).Color;
	PartMesh->SetStaticMesh(Mesh);
	Light->SetLightColor(Color);
	if (BeaconMesh && BeaconMaterial)
	{
		Beacon->SetStaticMesh(BeaconMesh);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BeaconMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Beacon->SetMaterial(0, MID);
	}
	else
	{
		Beacon->SetVisibility(false);
	}
}

void AShipPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	// local Z is the planet up: spin around it and bob along it
	PartMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, FMath::Sin(Clock * 2.f) * 12.f),
		FRotator(FMath::Sin(Clock * 0.7f) * 12.f, Clock * 60.f, 0.f));
	Light->SetIntensity(30.f + 10.f * FMath::Sin(Clock * 3.f));
}
