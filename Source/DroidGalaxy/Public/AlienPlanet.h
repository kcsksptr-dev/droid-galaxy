#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hazard.h"
#include "AlienPlanet.generated.h"

class ADirectionalLight;
class AHazard;
class APowerUp;
class AShipPart;
class ASpaceship;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A small spherical planet with radial gravity. Placed once in the level by Tools/build_alien.py.
 *
 * - Anything that wants planet gravity asks UpAt() (the droid applies its own gravity force).
 * - The sun follows the player so it is always mid-afternoon wherever KDS-3-21 drives.
 * - On BeginPlay it spawns the 5 ship parts at seeded random spots (seed = planet number, taken from the
 *   "?Planet=N" travel option) and tints the ground with that planet's palette.
 */
UCLASS()
class DROIDGALAXY_API AAlienPlanet : public AActor
{
	GENERATED_BODY()

public:
	AAlienPlanet();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AAlienPlanet* Find(const UObject* WorldContext);

	FVector UpAt(const FVector& Location) const { return (Location - GetActorLocation()).GetSafeNormal(); }

	/** Line trace against the planet surface along a direction from the centre. */
	bool ProjectToSurface(const FVector& Direction, FVector& OutPoint, FVector& OutNormal) const;

	FString GetPlanetName() const;
	int32 GetPlanetNumber() const;
	ASpaceship* GetShip() const { return Ship; }
	const TArray<TObjectPtr<AShipPart>>& GetParts() const { return Parts; }
	UStaticMesh* GetPartMesh(int32 Index) const { return PartMeshes.IsValidIndex(Index) ? PartMeshes[Index] : nullptr; }
	const TArray<TObjectPtr<AHazard>>& GetHazards() const { return Hazards; }
	const TArray<TObjectPtr<APowerUp>>& GetPowerUps() const { return PowerUps; }
	/** Name of the challenge built around a part (HUD/telemetry). */
	FString GetChallengeName(int32 PartIndex) const;

	/** Travel to the same map as the next planet (new part layout and palette). */
	void TravelToNextPlanet();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	TObjectPtr<UStaticMeshComponent> Surface;

	UPROPERTY(EditAnywhere, Category = "Planet")
	float Radius = 4500.f;

	UPROPERTY(EditAnywhere, Category = "Planet")
	float Gravity = 980.f;

	/** Sun height above the local horizon of the player (degrees). */
	UPROPERTY(EditAnywhere, Category = "Planet")
	float SunElevation = 50.f;

	UPROPERTY(EditAnywhere, Category = "Planet")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(EditAnywhere, Category = "Planet")
	TObjectPtr<ASpaceship> Ship;

	/** One mesh per Mission part index. */
	UPROPERTY(EditAnywhere, Category = "Planet|Mission")
	TArray<TObjectPtr<UStaticMesh>> PartMeshes;

	/** Tall light column over each part so it can be seen from afar. */
	UPROPERTY(EditAnywhere, Category = "Planet|Mission")
	TObjectPtr<UStaticMesh> BeaconMesh;

	UPROPERTY(EditAnywhere, Category = "Planet|Mission")
	TObjectPtr<UMaterialInterface> BeaconMaterial;

	/** Parts are at least this far (radians of arc) from the ship and from each other. */
	UPROPERTY(EditAnywhere, Category = "Planet|Hazards")
	TArray<TObjectPtr<UStaticMesh>> CactusMeshes;

	UPROPERTY(EditAnywhere, Category = "Planet|Hazards")
	TObjectPtr<UStaticMesh> SpikeMesh;

	UPROPERTY(EditAnywhere, Category = "Planet|Hazards")
	TObjectPtr<UStaticMesh> LavaMesh;

	UPROPERTY(EditAnywhere, Category = "Planet|Hazards")
	TObjectPtr<UStaticMesh> WallMesh;

	UPROPERTY(EditAnywhere, Category = "Planet|Hazards")
	TObjectPtr<UStaticMesh> VentMesh;

	/** Heal, Shield, Blaster (EPowerUpKind order). */
	UPROPERTY(EditAnywhere, Category = "Planet|PowerUps")
	TArray<TObjectPtr<UStaticMesh>> PowerUpMeshes;

	/** Sphere used for the shield bubble and blaster bolts (with BeaconMaterial). */
	UPROPERTY(EditAnywhere, Category = "Planet|PowerUps")
	TObjectPtr<UStaticMesh> EffectSphereMesh;

	UPROPERTY(EditAnywhere, Category = "Planet|Mission")
	float MinAngleFromShip = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Planet|Mission")
	float MinAngleBetweenParts = 0.75f;

private:
	void SpawnParts();
	void BuildChallenge(int32 PartIndex, const FVector& CenterDir, FRandomStream& Rng);
	void SpawnWildHazards(FRandomStream& Rng);
	void SpawnPowerUps(FRandomStream& Rng);
	/** Surface direction at (Radius-relative) polar offset Dist cm / Angle rad around CenterDir. */
	FVector Around(const FVector& CenterDir, float Dist, float Angle) const;
	AHazard* SpawnHazard(EHazardKind Kind, const FVector& Dir, float Scale, FRandomStream& Rng, const FVector* Tangent = nullptr);
	bool FarFromMissionSpots(const FVector& Dir, float MinAngle) const;
	void ApplyPalette();
	void UpdateSun();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AShipPart>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHazard>> Hazards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APowerUp>> PowerUps;

	TArray<FVector> PartDirs;

	mutable int32 PlanetNumber = 0; // 0 = not read from the travel options yet
	FVector SunTangent = FVector::ForwardVector;
};
