#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MissionHUD.generated.h"

class ARollerDroid;

/**
 * Canvas HUD for the planet mission (no UMG assets needed):
 *  - top-left: droid / planet name and the current objective
 *  - top-right: round minimap of the whole planet (azimuthal projection centred on the droid, view-up =
 *    camera heading; the rim is the far side of the planet) with the ship and remaining parts
 *  - bottom-left: inventory of the 5 parts (missing / carried / installed)
 *  - centre: interaction prompt, mission messages and the launch banner
 */
UCLASS()
class DROIDGALAXY_API AMissionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color = FLinearColor(0.02f, 0.02f, 0.06f, 0.55f));
	void DrawDisc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Segments = 48);
	void DrawRing(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments = 64);
	void DrawTri(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentered = false);

	void DrawObjective(ARollerDroid* Droid);
	void DrawMinimap(ARollerDroid* Droid);
	void DrawInventory(ARollerDroid* Droid);
	void DrawCenter(ARollerDroid* Droid);
	void DrawVitals(ARollerDroid* Droid);

	float S = 1.f; // UI scale (1 at 1080p)
};
