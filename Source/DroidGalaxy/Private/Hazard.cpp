#include "Hazard.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RollerDroid.h"

namespace
{
	constexpr float VentWarn = 1.3f;   // seconds of warning glow before an eruption
	constexpr float VentBlast = 1.1f;  // eruption duration
}

AHazard::AHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	Column = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Column"));
	Column->SetupAttachment(Mesh);
	Column->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Column->SetCastShadow(false);
	Column->SetVisibility(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetCastShadows(false);
	Light->SetVisibility(false);
}

void AHazard::Configure(EHazardKind InKind, UStaticMesh* InMesh, UStaticMesh* ColumnMesh, UMaterialInterface* ColumnMaterial)
{
	Kind = InKind;
	Mesh->SetStaticMesh(InMesh);
	BaseScale = GetActorScale3D();
	// solid hazards are world geometry (the droid's contact query and the blaster look for WorldStatic)
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	switch (Kind)
	{
	case EHazardKind::Spikes:
		// roll through them; the droid's overlap query still finds them
		Mesh->SetCollisionProfileName(TEXT("OverlapAll"));
		break;
	case EHazardKind::Vent:
		Mesh->SetCollisionProfileName(TEXT("NoCollision")); // the vent checks the droid itself
		if (ColumnMesh && ColumnMaterial)
		{
			Column->SetStaticMesh(ColumnMesh);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ColumnMaterial, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.4f, 0.08f));
			Column->SetMaterial(0, MID);
		}
		Light->SetLightColor(FLinearColor(1.f, 0.4f, 0.1f));
		Light->SetAttenuationRadius(700.f);
		Light->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		Light->SetVisibility(true);
		Period = FMath::FRandRange(4.f, 7.f);
		Clock = FMath::FRandRange(0.f, Period);
		SetActorTickEnabled(true);
		break;
	case EHazardKind::Lava:
		Light->SetLightColor(FLinearColor(1.f, 0.35f, 0.05f));
		Light->SetIntensity(25.f);
		Light->SetAttenuationRadius(500.f * BaseScale.X);
		Light->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
		Light->SetVisibility(true);
		break;
	default:
		break;
	}
}

void AHazard::Shatter()
{
	if (ShatterTime < 0.f)
	{
		ShatterTime = 0.f;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorTickEnabled(true);
	}
}

void AHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ShatterTime >= 0.f)
	{
		ShatterTime += DeltaSeconds;
		const float K = 1.f - ShatterTime / 0.35f;
		if (K <= 0.f)
		{
			Destroy();
			return;
		}
		SetActorScale3D(BaseScale * FVector(1.f + (1.f - K) * 0.6f, 1.f + (1.f - K) * 0.6f, K));
		return;
	}
	if (Kind == EHazardKind::Vent)
	{
		TickVent(DeltaSeconds);
	}
}

void AHazard::TickVent(float DeltaSeconds)
{
	Clock += DeltaSeconds;
	if (Clock > Period)
	{
		Clock -= Period;
		bHitThisEruption = false;
	}
	const float BlastStart = Period - VentBlast;
	const float WarnStart = BlastStart - VentWarn;
	const bool bWarn = Clock >= WarnStart && Clock < BlastStart;
	const bool bBlast = Clock >= BlastStart;
	if (bBlast)
	{
		const float T = (Clock - BlastStart) / VentBlast;
		const float H = FMath::Sin(T * PI);   // column rises and falls
		Column->SetVisibility(true);
		// engine cylinder: 100 cm tall, centred; make a column 50 cm wide up to 7 m tall standing on the vent
		Column->SetRelativeScale3D(FVector(0.5f + 0.3f * H, 0.5f + 0.3f * H, FMath::Max(0.05f, 7.f * H)));
		Column->SetRelativeLocation(FVector(0.f, 0.f, 350.f * H));
		Light->SetIntensity(150.f * H + 20.f);
		if (!bHitThisEruption)
		{
			ARollerDroid* Droid = Cast<ARollerDroid>(UGameplayStatics::GetPlayerPawn(this, 0));
			if (Droid)
			{
				const FVector Up = GetActorUpVector();
				const FVector ToDroid = Droid->GetActorLocation() - GetActorLocation();
				const float Height = FVector::DotProduct(ToDroid, Up);
				const float Lateral = (ToDroid - Up * Height).Size();
				if (Lateral < 130.f * BaseScale.X && Height < 400.f)
				{
					bHitThisEruption = true;
					Droid->ApplyHazardDamage(18.f, GetActorLocation(), TEXT("Vent eruption!"), Up * 950.f);
				}
			}
		}
	}
	else
	{
		Column->SetVisibility(false);
		const float Warn = bWarn ? (Clock - WarnStart) / VentWarn : 0.f;
		// idle: a lazy flicker; warning: fast bright pulse
		Light->SetIntensity(bWarn ? 20.f + 60.f * Warn * (0.6f + 0.4f * FMath::Sin(Clock * 30.f))
			: 6.f + 3.f * FMath::Sin(Clock * 2.f));
	}
}
