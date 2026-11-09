#include "AlienPlanet.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MissionTypes.h"
#include "PowerUp.h"
#include "ShipPart.h"
#include "Spaceship.h"

AAlienPlanet::AAlienPlanet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetMobility(EComponentMobility::Static);
	RootComponent = Surface;
}

AAlienPlanet* AAlienPlanet::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAlienPlanet> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AAlienPlanet::BeginPlay()
{
	Super::BeginPlay();
	if (!Sun)
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			Sun = *It;
			break;
		}
	}
	if (!Ship)
	{
		for (TActorIterator<ASpaceship> It(GetWorld()); It; ++It)
		{
			Ship = *It;
			break;
		}
	}
	ApplyPalette();
	SpawnParts();
	FRandomStream Rng(GetPlanetNumber() * 104729 + 7);
	SpawnWildHazards(Rng);
	SpawnPowerUps(Rng);
	UE_LOG(LogTemp, Display, TEXT("[Planet] %s (#%d): %d parts, %d hazards, %d power-ups"), *GetPlanetName(),
		GetPlanetNumber(), Parts.Num(), Hazards.Num(), PowerUps.Num());
}

int32 AAlienPlanet::GetPlanetNumber() const
{
	if (PlanetNumber == 0)
	{
		const AGameModeBase* GM = UGameplayStatics::GetGameMode(this);
		PlanetNumber = GM ? FMath::Max(1, UGameplayStatics::GetIntOption(GM->OptionsString, TEXT("Planet"), 1)) : 1;
	}
	return PlanetNumber;
}

FString AAlienPlanet::GetPlanetName() const
{
	return Mission::PlanetName(GetPlanetNumber());
}

void AAlienPlanet::ApplyPalette()
{
	struct FPalette { FLinearColor A, B, Glow; };
	static const FPalette Palettes[] = {
		{FLinearColor(0.55f, 0.45f, 0.78f), FLinearColor(0.95f, 0.38f, 0.22f), FLinearColor(0.1f, 1.f, 0.8f)},
		{FLinearColor(0.35f, 0.62f, 0.58f), FLinearColor(0.9f, 0.75f, 0.3f), FLinearColor(1.f, 0.3f, 0.8f)},
		{FLinearColor(0.8f, 0.58f, 0.42f), FLinearColor(0.3f, 0.32f, 0.75f), FLinearColor(0.5f, 1.f, 0.2f)},
		{FLinearColor(0.5f, 0.5f, 0.56f), FLinearColor(0.75f, 0.2f, 0.52f), FLinearColor(1.f, 0.6f, 0.1f)},
	};
	const FPalette& P = Palettes[(GetPlanetNumber() - 1) % UE_ARRAY_COUNT(Palettes)];
	if (UMaterialInstanceDynamic* MID = Surface->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("TintA"), P.A);
		MID->SetVectorParameterValue(TEXT("TintB"), P.B);
		MID->SetVectorParameterValue(TEXT("CrackGlow"), P.Glow);
	}
}

bool AAlienPlanet::ProjectToSurface(const FVector& Direction, FVector& OutPoint, FVector& OutNormal) const
{
	const FVector Dir = Direction.GetSafeNormal();
	const FVector Center = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlanetSurface), true);
	if (Surface->LineTraceComponent(Hit, Center + Dir * Radius * 2.f, Center, Params))
	{
		OutPoint = Hit.ImpactPoint;
		OutNormal = Hit.ImpactNormal;
		return true;
	}
	OutPoint = Center + Dir * Radius;
	OutNormal = Dir;
	return false;
}

void AAlienPlanet::SpawnParts()
{
	FRandomStream Rng(GetPlanetNumber() * 7919 + 13);
	const FVector ShipUp = Ship ? UpAt(Ship->GetActorLocation()) : FVector::UpVector;
	TArray<FVector> Dirs;
	for (int32 Index = 0; Index < Mission::NumParts; ++Index)
	{
		for (int32 Try = 0; Try < 400; ++Try)
		{
			const FVector Dir = Rng.VRand();
			const float FromShip = FMath::Acos(FVector::DotProduct(Dir, ShipUp));
			if (FromShip < MinAngleFromShip || FromShip > PI - 0.25f)
			{
				continue;
			}
			// relax the spacing rule if the planet is crowded
			const float MinSep = MinAngleBetweenParts * (Try < 250 ? 1.f : 0.5f);
			if (Dirs.ContainsByPredicate([&](const FVector& D) { return FMath::Acos(FVector::DotProduct(D, Dir)) < MinSep; }))
			{
				continue;
			}
			FVector Point, Normal;
			ProjectToSurface(Dir, Point, Normal);
			const FVector Up = UpAt(Point);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PartSpot), false, this);
			if (Try < 350 && GetWorld()->OverlapAnyTestByChannel(Point + Up * 90.f, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(120.f), Params))
			{
				continue; // inside a crystal cluster or tree: try elsewhere
			}
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FRotator Rotation = FRotationMatrix::MakeFromZ(Up).Rotator();
			if (AShipPart* Part = GetWorld()->SpawnActor<AShipPart>(Point + Up * 75.f, Rotation, SpawnParams))
			{
				Part->Init(Index, GetPartMesh(Index), BeaconMesh, BeaconMaterial);
				Parts.Add(Part);
				Dirs.Add(Dir);
				PartDirs.Add(Dir);
				BuildChallenge(Index, Dir, Rng);
			}
			break;
		}
	}
}

void AAlienPlanet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSun();
}

void AAlienPlanet::UpdateSun()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Sun || !Player)
	{
		return;
	}
	const FVector Up = UpAt(Player->GetActorLocation());
	FVector T = FVector::VectorPlaneProject(SunTangent, Up);
	if (!T.Normalize())
	{
		T = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
	}
	SunTangent = T;
	const float E = FMath::DegreesToRadians(SunElevation);
	const FVector ToSun = Up * FMath::Sin(E) + T * FMath::Cos(E);
	Sun->SetActorRotation((-ToSun).Rotation());
}

void AAlienPlanet::TravelToNextPlanet()
{
	const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*Map), true, FString::Printf(TEXT("Planet=%d"), GetPlanetNumber() + 1));
}

// ----------------------------------------------------------------------------- hazards, challenges, power-ups

namespace
{
	enum class EChallenge : int32 { LavaMoat, CactusRing, SpikeField, WallRing, VentField, Count };

	EChallenge ChallengeFor(int32 PartIndex, int32 PlanetNumber)
	{
		return static_cast<EChallenge>((PartIndex + PlanetNumber - 1) % static_cast<int32>(EChallenge::Count));
	}

	/** Two unit tangents perpendicular to Up. */
	void TangentBasis(const FVector& Up, FVector& T1, FVector& T2)
	{
		T1 = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		T2 = FVector::CrossProduct(Up, T1);
	}
}

FString AAlienPlanet::GetChallengeName(int32 PartIndex) const
{
	switch (ChallengeFor(PartIndex, GetPlanetNumber()))
	{
	case EChallenge::LavaMoat: return TEXT("lava moat");
	case EChallenge::CactusRing: return TEXT("cactus thicket");
	case EChallenge::SpikeField: return TEXT("thorn field");
	case EChallenge::WallRing: return TEXT("basalt walls");
	default: return TEXT("volatile vents");
	}
}

FVector AAlienPlanet::Around(const FVector& CenterDir, float Dist, float Angle) const
{
	FVector T1, T2;
	TangentBasis(CenterDir, T1, T2);
	return (CenterDir * Radius + (T1 * FMath::Cos(Angle) + T2 * FMath::Sin(Angle)) * Dist).GetSafeNormal();
}

AHazard* AAlienPlanet::SpawnHazard(EHazardKind Kind, const FVector& Dir, float Scale, FRandomStream& Rng, const FVector* Tangent)
{
	UStaticMesh* Mesh = nullptr;
	switch (Kind)
	{
	case EHazardKind::Cactus: Mesh = CactusMeshes.Num() ? CactusMeshes[Rng.RandHelper(CactusMeshes.Num())].Get() : nullptr; break;
	case EHazardKind::Spikes: Mesh = SpikeMesh; break;
	case EHazardKind::Lava: Mesh = LavaMesh; break;
	case EHazardKind::Wall: Mesh = WallMesh; break;
	case EHazardKind::Vent: Mesh = VentMesh; break;
	}
	if (!Mesh)
	{
		return nullptr;
	}
	FVector Point, Normal;
	ProjectToSurface(Dir, Point, Normal);
	const FVector Radial = UpAt(Point);
	// plants stand radially; flat things (lava, vents, walls) follow the ground
	const FVector Up = Kind == EHazardKind::Cactus ? Radial : (Radial * 0.4f + Normal * 0.6f).GetSafeNormal();
	FVector X = Tangent ? FVector::VectorPlaneProject(*Tangent, Up) : FVector::VectorPlaneProject(Rng.VRand(), Up);
	X.Normalize();
	const float Sink = Kind == EHazardKind::Lava ? 3.f : Kind == EHazardKind::Wall ? 8.f : 4.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Xf(FRotationMatrix::MakeFromZX(Up, X).ToQuat(), Point - Up * Sink, FVector(Scale));
	AHazard* Hazard = GetWorld()->SpawnActorDeferred<AHazard>(AHazard::StaticClass(), Xf, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Hazard)
	{
		return nullptr;
	}
	Hazard->FinishSpawning(Xf);
	Hazard->Configure(Kind, Mesh, BeaconMesh, BeaconMaterial); // vent column = beacon cylinder
	Hazards.Add(Hazard);
	return Hazard;
}

void AAlienPlanet::BuildChallenge(int32 PartIndex, const FVector& C, FRandomStream& Rng)
{
	switch (ChallengeFor(PartIndex, GetPlanetNumber()))
	{
	case EChallenge::LavaMoat:
		// a ring of overlapping pools: jump across
		for (int32 i = 0; i < 11; ++i)
		{
			SpawnHazard(EHazardKind::Lava, Around(C, 330.f, 2.f * PI * i / 11.f), 1.35f, Rng);
		}
		break;
	case EChallenge::CactusRing:
		// tight thicket with narrow gaps, plus a few outliers
		for (int32 i = 0; i < 13; ++i)
		{
			const float A = 2.f * PI * i / 13.f + Rng.FRandRange(-0.08f, 0.08f);
			SpawnHazard(EHazardKind::Cactus, Around(C, Rng.FRandRange(250.f, 300.f), A), Rng.FRandRange(0.9f, 1.2f), Rng);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			SpawnHazard(EHazardKind::Cactus, Around(C, Rng.FRandRange(420.f, 650.f), Rng.FRandRange(0.f, 2.f * PI)), Rng.FRandRange(0.8f, 1.2f), Rng);
		}
		break;
	case EChallenge::SpikeField:
	{
		// dense thorns all round: roll through slowly (careful mode) or get hurt
		TArray<FVector2D> Placed;
		for (int32 Try = 0; Try < 400 && Placed.Num() < 70; ++Try)
		{
			const FVector2D P = FVector2D(Rng.FRandRange(-600.f, 600.f), Rng.FRandRange(-600.f, 600.f));
			const float D = P.Size();
			if (D < 110.f || D > 600.f || Placed.ContainsByPredicate([&](const FVector2D& Q) { return FVector2D::Distance(P, Q) < 75.f; }))
			{
				continue;
			}
			Placed.Add(P);
			SpawnHazard(EHazardKind::Spikes, Around(C, D, FMath::Atan2(P.Y, P.X)), Rng.FRandRange(0.9f, 1.4f), Rng);
		}
		break;
	}
	case EChallenge::WallRing:
	{
		// closed ring of knee-high basalt walls (jump over) with two vents inside
		FVector T1, T2;
		TangentBasis(C, T1, T2);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = 2.f * PI * i / 9.f;
			const FVector Tangent = -T1 * FMath::Sin(A) + T2 * FMath::Cos(A);
			SpawnHazard(EHazardKind::Wall, Around(C, 400.f, A), 1.0f, Rng, &Tangent);
		}
		for (int32 i = 0; i < 2; ++i)
		{
			SpawnHazard(EHazardKind::Vent, Around(C, 200.f, PI * i + 0.6f), 1.f, Rng);
		}
		break;
	}
	default:
		// volatile ground: vents all round, pools of lava further out
		for (int32 i = 0; i < 7; ++i)
		{
			SpawnHazard(EHazardKind::Vent, Around(C, Rng.FRandRange(150.f, 430.f), 2.f * PI * i / 7.f + Rng.FRandRange(-0.2f, 0.2f)), Rng.FRandRange(0.9f, 1.2f), Rng);
		}
		for (int32 i = 0; i < 3; ++i)
		{
			SpawnHazard(EHazardKind::Lava, Around(C, 560.f, 2.f * PI * i / 3.f + 0.5f), 1.1f, Rng);
		}
		break;
	}
}

bool AAlienPlanet::FarFromMissionSpots(const FVector& Dir, float MinAngle) const
{
	const FVector ShipUp = Ship ? UpAt(Ship->GetActorLocation()) : FVector::UpVector;
	if (FMath::Acos(FVector::DotProduct(Dir, ShipUp)) < 0.22f)
	{
		return false;
	}
	return !PartDirs.ContainsByPredicate([&](const FVector& D) { return FMath::Acos(FVector::DotProduct(D, Dir)) < MinAngle; });
}

void AAlienPlanet::SpawnWildHazards(FRandomStream& Rng)
{
	auto Spot = [&](float MinAngle)
	{
		for (int32 Try = 0; Try < 100; ++Try)
		{
			const FVector Dir = Rng.VRand();
			if (FarFromMissionSpots(Dir, MinAngle))
			{
				return Dir;
			}
		}
		return Rng.VRand();
	};
	for (int32 i = 0; i < 30; ++i)
	{
		SpawnHazard(EHazardKind::Cactus, Spot(0.2f), Rng.FRandRange(0.8f, 1.3f), Rng);
	}
	for (int32 i = 0; i < 12; ++i)
	{
		SpawnHazard(EHazardKind::Vent, Spot(0.2f), Rng.FRandRange(0.9f, 1.3f), Rng);
	}
	for (int32 i = 0; i < 5; ++i)
	{
		SpawnHazard(EHazardKind::Lava, Spot(0.2f), Rng.FRandRange(1.0f, 1.8f), Rng);
	}
	for (int32 Patch = 0; Patch < 4; ++Patch)
	{
		const FVector C = Spot(0.25f);
		for (int32 i = 0; i < 14; ++i)
		{
			SpawnHazard(EHazardKind::Spikes, Around(C, Rng.FRandRange(0.f, 300.f), Rng.FRandRange(0.f, 2.f * PI)), Rng.FRandRange(0.8f, 1.3f), Rng);
		}
	}
}

void AAlienPlanet::SpawnPowerUps(FRandomStream& Rng)
{
	if (PowerUpMeshes.Num() < 3)
	{
		return;
	}
	auto Place = [&](EPowerUpKind Kind, const FVector& Dir)
	{
		FVector Point, Normal;
		ProjectToSurface(Dir, Point, Normal);
		const FVector Up = UpAt(Point);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APowerUp* P = GetWorld()->SpawnActor<APowerUp>(Point + Up * 70.f, FRotationMatrix::MakeFromZ(Up).Rotator(), Params))
		{
			P->Configure(Kind, PowerUpMeshes[static_cast<int32>(Kind)]);
			PowerUps.Add(P);
		}
	};
	// a blaster and a shield near the landing site, so the player meets them early
	const FVector ShipUp = Ship ? UpAt(Ship->GetActorLocation()) : FVector::UpVector;
	Place(EPowerUpKind::Blaster, Around(ShipUp, 900.f, 0.45f));
	Place(EPowerUpKind::Shield, Around(ShipUp, 900.f, -0.9f));
	const EPowerUpKind Wild[] = {EPowerUpKind::Heal, EPowerUpKind::Heal, EPowerUpKind::Heal, EPowerUpKind::Heal,
		EPowerUpKind::Heal, EPowerUpKind::Heal, EPowerUpKind::Shield, EPowerUpKind::Shield, EPowerUpKind::Shield,
		EPowerUpKind::Blaster, EPowerUpKind::Blaster, EPowerUpKind::Blaster};
	for (EPowerUpKind Kind : Wild)
	{
		for (int32 Try = 0; Try < 100; ++Try)
		{
			const FVector Dir = Rng.VRand();
			// near (but outside) a challenge is the most useful place; otherwise anywhere open
			if (FarFromMissionSpots(Dir, 0.17f))
			{
				Place(Kind, Dir);
				break;
			}
		}
	}
}
