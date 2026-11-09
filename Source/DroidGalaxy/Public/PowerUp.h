#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowerUp.generated.h"

class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EPowerUpKind : uint8
{
	Heal,
	Shield,
	Blaster,
};

/** Floating pickup; collected automatically when the droid rolls into it. */
UCLASS()
class DROIDGALAXY_API APowerUp : public AActor
{
	GENERATED_BODY()

public:
	APowerUp();
	void Configure(EPowerUpKind InKind, UStaticMesh* InMesh);
	virtual void Tick(float DeltaSeconds) override;

	EPowerUpKind GetKind() const { return Kind; }
	static FLinearColor KindColor(EPowerUpKind Kind);
	static const TCHAR* KindName(EPowerUpKind Kind);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	EPowerUpKind Kind = EPowerUpKind::Heal;
	float Clock = 0.f;
};
