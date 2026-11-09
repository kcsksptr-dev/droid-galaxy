#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hazard.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EHazardKind : uint8
{
	Cactus,  // solid; hurts and knocks back on any contact
	Spikes,  // roll-through thorn field; hurts only above SpikeSafeSpeed (go slow / careful mode)
	Lava,    // solid pool surface; burns every second you touch it
	Vent,    // volatile ground: glows, then erupts, throwing the droid up and hurting it
	Wall,    // jumpable obstacle, harmless
};

/** Spawned by AAlienPlanet around the ship parts and across the planet. The droid looks for these when touching things. */
UCLASS()
class DROIDGALAXY_API AHazard : public AActor
{
	GENERATED_BODY()

public:
	AHazard();

	void Configure(EHazardKind InKind, UStaticMesh* Mesh, UStaticMesh* ColumnMesh = nullptr, UMaterialInterface* ColumnMaterial = nullptr);
	virtual void Tick(float DeltaSeconds) override;

	EHazardKind GetKind() const { return Kind; }
	bool CanBeShot() const { return Kind == EHazardKind::Cactus || Kind == EHazardKind::Spikes || Kind == EHazardKind::Wall; }
	/** Blaster hit: shrink away, then destroy. */
	void Shatter();
	bool IsShattering() const { return ShatterTime >= 0.f; }

	static constexpr float SpikeSafeSpeed = 230.f;

private:
	void TickVent(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Vents only: the eruption column and its light. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Column;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	EHazardKind Kind = EHazardKind::Wall;
	float Clock = 0.f;
	float Period = 5.f;
	bool bHitThisEruption = false;
	float ShatterTime = -1.f;
	FVector BaseScale = FVector::OneVector;
};
