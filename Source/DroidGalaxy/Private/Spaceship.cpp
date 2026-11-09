#include "Spaceship.h"

#include "AlienPlanet.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MissionTypes.h"
#include "RollerDroid.h"

ASpaceship::ASpaceship()
{
	PrimaryActorTick.bCanEverTick = true;
	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetMobility(EComponentMobility::Movable);
	RootComponent = Hull;

	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Hull);
	Flame->SetRelativeLocation(FVector(0.f, 0.f, 25.f)); // nozzle exit
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetCastShadow(false);
	Flame->SetVisibility(false);

	EngineLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EngineLight"));
	EngineLight->SetupAttachment(Hull);
	EngineLight->SetRelativeLocation(FVector(0.f, 0.f, -60.f));
	EngineLight->SetIntensityUnits(ELightUnits::Candelas);
	EngineLight->SetIntensity(0.f);
	EngineLight->SetAttenuationRadius(2500.f);
	EngineLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
}

int32 ASpaceship::NumInstalled() const
{
	return FMath::CountBits(static_cast<uint64>(InstalledMask));
}

bool ASpaceship::IsRepaired() const
{
	return NumInstalled() == Mission::NumParts;
}

int32 ASpaceship::InstallParts(TArray<int32>& Inventory)
{
	AAlienPlanet* Planet = AAlienPlanet::Find(this);
	int32 Count = 0;
	for (int32 Index : Inventory)
	{
		if (IsInstalled(Index))
		{
			continue;
		}
		InstalledMask |= 1 << Index;
		++Count;
		const Mission::FPartInfo& Info = Mission::Part(Index);
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetStaticMesh(Planet ? Planet->GetPartMesh(Index) : nullptr);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetupAttachment(Hull);
		Comp->SetRelativeLocationAndRotation(Info.SocketLocation, FRotator(0.f, Info.SocketYaw, 0.f));
		Comp->SetRelativeScale3D(FVector(1.3f));
		Comp->RegisterComponent();
	}
	Inventory.Reset();
	return Count;
}

void ASpaceship::Launch(ARollerDroid* Pilot)
{
	if (bLaunching || !IsRepaired())
	{
		return;
	}
	bLaunching = true;
	LaunchClock = 0.f;
	Speed = 0.f;
	AAlienPlanet* Planet = AAlienPlanet::Find(this);
	LaunchUp = Planet ? Planet->UpAt(GetActorLocation()) : GetActorUpVector();
	Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetVisibility(true);
}

void ASpaceship::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLaunching)
	{
		return;
	}
	LaunchClock += DeltaSeconds;
	const float Flicker = 0.85f + 0.3f * FMath::PerlinNoise1D(LaunchClock * 17.f);
	const float Ignite = FMath::Clamp(LaunchClock / 1.2f, 0.f, 1.f);
	Flame->SetRelativeScale3D(FVector(Ignite * Flicker, Ignite * Flicker, Ignite * (0.6f + Flicker)));
	EngineLight->SetIntensity(400.f * Ignite * Flicker);
	if (LaunchClock > 1.5f) // engines spool up, then lift off and accelerate away
	{
		Speed += DeltaSeconds * FMath::Min(250.f + 400.f * (LaunchClock - 1.5f), 2500.f);
		AddActorWorldOffset(LaunchUp * Speed * DeltaSeconds);
		AddActorWorldRotation(FQuat(LaunchUp, DeltaSeconds * 0.6f)); // slow roll
	}
	if (!bTravelled && LaunchClock > TravelAfter)
	{
		bTravelled = true;
		if (AAlienPlanet* Planet = AAlienPlanet::Find(this))
		{
			Planet->TravelToNextPlanet();
		}
	}
}
