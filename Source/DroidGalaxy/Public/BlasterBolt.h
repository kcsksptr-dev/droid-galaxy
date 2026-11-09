#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlasterBolt.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Blaster shot: flies straight, sweeps each tick, shatters cacti / spikes / walls it hits. */
UCLASS()
class DROIDGALAXY_API ABlasterBolt : public AActor
{
	GENERATED_BODY()

public:
	ABlasterBolt();
	void Launch(const FVector& Direction, AActor* Shooter, UStaticMesh* Mesh, UMaterialInterface* Material);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
};
