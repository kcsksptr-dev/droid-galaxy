#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DroidGalaxyGameMode.generated.h"

/** Spawns the player as KDS-3-21 (the rolling droid) with the mission HUD. */
UCLASS()
class DROIDGALAXY_API ADroidGalaxyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADroidGalaxyGameMode();
};
