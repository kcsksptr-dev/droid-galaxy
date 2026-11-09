#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipPart.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One of the 5 lost ship parts: hovers, spins and glows under a light beacon until KDS-3-21 picks it up. */
UCLASS()
class DROIDGALAXY_API AShipPart : public AActor
{
	GENERATED_BODY()

public:
	AShipPart();

	void Init(int32 InPartIndex, UStaticMesh* Mesh, UStaticMesh* BeaconMesh, UMaterialInterface* BeaconMaterial);
	virtual void Tick(float DeltaSeconds) override;

	int32 GetPartIndex() const { return PartIndex; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PartMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	int32 PartIndex = 0;
	float Clock = 0.f;
};
