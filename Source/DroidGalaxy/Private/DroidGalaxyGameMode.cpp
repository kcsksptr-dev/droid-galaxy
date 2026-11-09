#include "DroidGalaxyGameMode.h"
#include "MissionHUD.h"
#include "RollerDroid.h"

ADroidGalaxyGameMode::ADroidGalaxyGameMode()
{
	DefaultPawnClass = ARollerDroid::StaticClass();
	HUDClass = AMissionHUD::StaticClass();
}
