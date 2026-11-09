#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spaceship.generated.h"

class ARollerDroid;
class UPointLightComponent;
class UStaticMeshComponent;

/** KDS-3-21's crashed ship. Install all 5 parts (E at the ship), then launch (E again) to fly to the next planet. */
UCLASS()
class DROIDGALAXY_API ASpaceship : public AActor
{
	GENERATED_BODY()

public:
	ASpaceship();
	virtual void Tick(float DeltaSeconds) override;

	bool IsInstalled(int32 PartIndex) const { return (InstalledMask & (1 << PartIndex)) != 0; }
	int32 NumInstalled() const;
	bool IsRepaired() const;
	bool IsLaunching() const { return bLaunching; }
	float GetLaunchTime() const { return LaunchClock; }

	/** Installs every carried part, removing them from Inventory. Returns how many were installed. */
	int32 InstallParts(TArray<int32>& Inventory);
	void Launch(ARollerDroid* Pilot);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Flame;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> EngineLight;

	/** Seconds after lift-off before travelling to the next planet. */
	UPROPERTY(EditAnywhere)
	float TravelAfter = 8.f;

private:
	int32 InstalledMask = 0;
	bool bLaunching = false;
	bool bTravelled = false;
	float LaunchClock = 0.f;
	float Speed = 0.f;
	FVector LaunchUp = FVector::UpVector;
};
