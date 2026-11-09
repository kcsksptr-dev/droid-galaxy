#include "MissionHUD.h"

#include "AlienPlanet.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "MissionTypes.h"
#include "PowerUp.h"
#include "RollerDroid.h"
#include "ShipPart.h"
#include "Spaceship.h"

namespace
{
	const FLinearColor Text(0.92f, 0.95f, 1.f, 1.f);
	const FLinearColor Dim(0.6f, 0.65f, 0.75f, 1.f);
	const FLinearColor Accent(0.25f, 1.f, 0.9f, 1.f);
}

void AMissionHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, W, H);
	DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.6f), X, Y, 3.f * S, H); // accent edge
}

void AMissionHUD::DrawDisc(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Segments)
{
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * PI * i / Segments, A1 = 2.f * PI * (i + 1) / Segments;
		FCanvasUVTri T;
		T.V0_Pos = Center;
		T.V1_Pos = Center + Radius * FVector2D(FMath::Cos(A0), FMath::Sin(A0));
		T.V2_Pos = Center + Radius * FVector2D(FMath::Cos(A1), FMath::Sin(A1));
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AMissionHUD::DrawRing(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * PI * i / Segments, A1 = 2.f * PI * (i + 1) / Segments;
		const FVector2D P0 = Center + Radius * FVector2D(FMath::Cos(A0), FMath::Sin(A0));
		const FVector2D P1 = Center + Radius * FVector2D(FMath::Cos(A1), FMath::Sin(A1));
		DrawLine(P0.X, P0.Y, P1.X, P1.Y, Color, Thickness);
	}
}

void AMissionHUD::DrawTri(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
{
	FCanvasUVTri T;
	T.V0_Pos = A;
	T.V1_Pos = B;
	T.V2_Pos = C;
	T.V0_Color = T.V1_Color = T.V2_Color = Color;
	FCanvasTriangleItem Item(TArray<FCanvasUVTri>{T}, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AMissionHUD::DrawTextAt(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentered)
{
	if (bCentered)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Str, W, H, Font, Scale);
		X -= W * 0.5f;
	}
	DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A), X + 1.5f * S, Y + 1.5f * S, Font, Scale); // drop shadow
	DrawText(Str, Color, X, Y, Font, Scale);
}

void AMissionHUD::DrawHUD()
{
	Super::DrawHUD();
	ARollerDroid* Droid = Cast<ARollerDroid>(GetOwningPawn());
	if (!Droid || !Canvas || !Droid->GetPlanet())
	{
		return;
	}
	S = FMath::Max(Canvas->ClipY / 1080.f, 0.6f);
	// red flash when hurt
	if (const float Flash = Droid->GetDamageFlash(); Flash > 0.f)
	{
		DrawRect(FLinearColor(0.8f, 0.f, 0.02f, 0.28f * Flash), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
	DrawObjective(Droid);
	DrawVitals(Droid);
	DrawMinimap(Droid);
	DrawInventory(Droid);
	DrawCenter(Droid);
}

void AMissionHUD::DrawObjective(ARollerDroid* Droid)
{
	AAlienPlanet* Planet = Droid->GetPlanet();
	ASpaceship* Ship = Planet->GetShip();
	const int32 Installed = Ship ? Ship->NumInstalled() : 0;
	const int32 Found = Installed + Droid->GetInventory().Num();

	FString Objective;
	if (Ship && Ship->IsLaunching())
	{
		Objective = TEXT("Leaving orbit...");
	}
	else if (Ship && Ship->IsRepaired())
	{
		Objective = TEXT("Ship repaired: press E at the ship to launch");
	}
	else if (Found == Mission::NumParts)
	{
		Objective = TEXT("All parts found: bring them to the ship");
	}
	else
	{
		Objective = FString::Printf(TEXT("Find the lost ship parts  (%d/%d found)"), Found, Mission::NumParts);
	}

	// distance to the nearest part still lying around (great-circle, metres)
	FString Nearest;
	float Best = TNumericLimits<float>::Max();
	const FVector Up = Droid->GetUp();
	for (AShipPart* Part : Planet->GetParts())
	{
		if (IsValid(Part))
		{
			const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, Planet->UpAt(Part->GetActorLocation())), -1.f, 1.f));
			Best = FMath::Min(Best, Angle * Planet->Radius / 100.f);
		}
	}
	if (Best < TNumericLimits<float>::Max())
	{
		Nearest = FString::Printf(TEXT("Nearest part signal: %.0f m"), Best);
	}
	else if (Ship && !Ship->IsLaunching())
	{
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, Planet->UpAt(Ship->GetActorLocation())), -1.f, 1.f));
		Nearest = FString::Printf(TEXT("Ship: %.0f m"), Angle * Planet->Radius / 100.f);
	}

	const float X = 24.f * S, Y = 24.f * S;
	DrawPanel(X, Y, 520.f * S, 118.f * S);
	DrawTextAt(FString::Printf(TEXT("%s   |   PLANET %s"), ARollerDroid::DroidName(), *Planet->GetPlanetName().ToUpper()),
		X + 16.f * S, Y + 10.f * S, Accent, GEngine->GetMediumFont(), 1.15f * S);
	DrawTextAt(Objective, X + 16.f * S, Y + 44.f * S, Text, GEngine->GetMediumFont(), 1.0f * S);
	DrawTextAt(Nearest, X + 16.f * S, Y + 76.f * S, Dim, GEngine->GetSmallFont(), 1.1f * S);
}

void AMissionHUD::DrawMinimap(ARollerDroid* Droid)
{
	AAlienPlanet* Planet = Droid->GetPlanet();
	const float R = 125.f * S;
	const FVector2D C(Canvas->ClipX - 24.f * S - R, 24.f * S + R);
	DrawDisc(C, R + 4.f * S, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	DrawDisc(C, R, FLinearColor(0.05f, 0.03f, 0.12f, 0.7f));
	DrawRing(C, R, FLinearColor(Accent.R, Accent.G, Accent.B, 0.8f), 2.f * S);
	DrawRing(C, R * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.15f), 1.f, 48);   // the planet's "equator" seen from here
	DrawLine(C.X - R, C.Y, C.X + R, C.Y, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f);
	DrawLine(C.X, C.Y - R, C.X, C.Y + R, FLinearColor(1.f, 1.f, 1.f, 0.08f), 1.f);

	// Azimuthal equidistant projection of the sphere around the droid: distance from the centre is the
	// great-circle angle (0..pi -> 0..R), direction is the bearing relative to the camera heading.
	const FVector Up = Droid->GetUp();
	const FVector Fwd = Droid->GetCameraForward();
	const FVector Right = FVector::CrossProduct(Up, Fwd);
	auto Project = [&](const FVector& WorldLocation)
	{
		const FVector D = Planet->UpAt(WorldLocation);
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, D), -1.f, 1.f));
		const FVector2D Bearing = FVector2D(FVector::DotProduct(D, Right), -FVector::DotProduct(D, Fwd)).GetSafeNormal();
		return C + Bearing * (Angle / PI) * (R - 8.f * S);
	};

	if (ASpaceship* Ship = Planet->GetShip())
	{
		const FVector2D P = Project(Ship->GetActorLocation());
		const float K = 9.f * S;
		DrawTri(P + FVector2D(0, -K * 1.3f), P + FVector2D(-K, K), P + FVector2D(K, K), FLinearColor(1.f, 0.6f, 0.2f, 1.f));
		DrawTextAt(TEXT("SHIP"), P.X, P.Y + K * 1.2f, FLinearColor(1.f, 0.7f, 0.35f, 1.f), GEngine->GetSmallFont(), 0.9f * S, true);
	}
	const float Pulse = 0.75f + 0.25f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	for (AShipPart* Part : Planet->GetParts())
	{
		if (IsValid(Part))
		{
			const FVector2D P = Project(Part->GetActorLocation());
			const FLinearColor Col = Mission::Part(Part->GetPartIndex()).Color;
			DrawDisc(P, 8.f * S * Pulse, FLinearColor(Col.R, Col.G, Col.B, 0.35f), 16);
			DrawDisc(P, 4.5f * S, FLinearColor(Col.R, Col.G, Col.B, 1.f), 16);
		}
	}
	for (APowerUp* P : Planet->GetPowerUps())
	{
		if (IsValid(P))
		{
			const FVector2D Q = Project(P->GetActorLocation());
			const FLinearColor Col = APowerUp::KindColor(P->GetKind());
			DrawRect(FLinearColor(Col.R, Col.G, Col.B, 0.9f), Q.X - 2.5f * S, Q.Y - 2.5f * S, 5.f * S, 5.f * S);
		}
	}
	// the droid: arrow pointing along the camera heading (always up on the map)
	const float K = 10.f * S;
	DrawTri(C + FVector2D(0, -K * 1.4f), C + FVector2D(-K * 0.8f, K), C + FVector2D(K * 0.8f, K), FLinearColor(1.f, 1.f, 1.f, 1.f));
	DrawTextAt(TEXT("PLANET MAP"), C.X, C.Y + R + 8.f * S, Dim, GEngine->GetSmallFont(), 0.95f * S, true);
}

void AMissionHUD::DrawInventory(ARollerDroid* Droid)
{
	ASpaceship* Ship = Droid->GetPlanet()->GetShip();
	const TArray<int32>& Inv = Droid->GetInventory();
	const float Row = 34.f * S;
	const float W = 340.f * S, H = 52.f * S + Row * Mission::NumParts;
	const float X = 24.f * S, Y = Canvas->ClipY - 24.f * S - H;
	DrawPanel(X, Y, W, H);
	DrawTextAt(FString::Printf(TEXT("INVENTORY   %d carried"), Inv.Num()), X + 16.f * S, Y + 10.f * S, Accent,
		GEngine->GetSmallFont(), 1.15f * S);
	for (int32 i = 0; i < Mission::NumParts; ++i)
	{
		const Mission::FPartInfo& Info = Mission::Part(i);
		const bool bInstalled = Ship && Ship->IsInstalled(i);
		const bool bCarried = Inv.Contains(i);
		const float RY = Y + 42.f * S + i * Row;
		const float Box = 22.f * S;
		// icon: coloured gem square when found, hollow when still missing
		if (bInstalled || bCarried)
		{
			DrawRect(Info.Color, X + 16.f * S, RY, Box, Box);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), X + 16.f * S + 4.f * S, RY + 4.f * S, 6.f * S, 6.f * S);
		}
		else
		{
			DrawRect(FLinearColor(Info.Color.R, Info.Color.G, Info.Color.B, 0.18f), X + 16.f * S, RY, Box, Box);
			DrawRect(FLinearColor(0.02f, 0.02f, 0.06f, 0.9f), X + 18.f * S, RY + 2.f * S, Box - 4.f * S, Box - 4.f * S);
		}
		const FLinearColor NameColor = (bInstalled || bCarried) ? Text : Dim;
		DrawTextAt(Info.Name, X + 50.f * S, RY + 1.f * S, NameColor, GEngine->GetSmallFont(), 1.1f * S);
		const FString State = bInstalled ? TEXT("INSTALLED") : bCarried ? TEXT("CARRIED") : TEXT("MISSING");
		const FLinearColor StateColor = bInstalled ? FLinearColor(0.4f, 1.f, 0.5f) : bCarried ? FLinearColor(1.f, 0.85f, 0.3f)
			: FLinearColor(0.75f, 0.4f, 0.45f);
		float TW = 0.f, TH = 0.f;
		GetTextSize(State, TW, TH, GEngine->GetSmallFont(), 0.95f * S);
		DrawTextAt(State, X + W - 16.f * S - TW, RY + 2.f * S, StateColor, GEngine->GetSmallFont(), 0.95f * S);
	}
}

void AMissionHUD::DrawCenter(ARollerDroid* Droid)
{
	const float CX = Canvas->ClipX * 0.5f;
	UFont* Medium = GEngine->GetMediumFont();

	const FString Prompt = Droid->GetInteractPrompt();
	if (!Prompt.IsEmpty())
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Prompt, W, H, Medium, 1.2f * S);
		const float Y = Canvas->ClipY * 0.68f;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.06f, 0.65f), CX - W * 0.5f - 18.f * S, Y - 8.f * S, W + 36.f * S, H + 16.f * S);
		DrawTextAt(Prompt, CX, Y, FLinearColor(1.f, 0.9f, 0.4f), Medium, 1.2f * S, true);
	}

	const FString Msg = Droid->GetMessage();
	if (!Msg.IsEmpty())
	{
		const float Fade = FMath::Clamp(Droid->GetMessageAge() * 4.f, 0.f, 1.f);
		float W = 0.f, H = 0.f;
		GetTextSize(Msg, W, H, Medium, 1.15f * S);
		const float Y = 170.f * S;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.06f, 0.55f * Fade), CX - W * 0.5f - 20.f * S, Y - 8.f * S, W + 40.f * S, H + 16.f * S);
		DrawTextAt(Msg, CX, Y, FLinearColor(Text.R, Text.G, Text.B, Fade), Medium, 1.15f * S, true);
	}

	ASpaceship* Ship = Droid->GetPlanet()->GetShip();
	if (Ship && Ship->IsLaunching() && Ship->GetLaunchTime() > 4.f)
	{
		const float A = FMath::Clamp((Ship->GetLaunchTime() - 4.f) / 1.5f, 0.f, 1.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.02f, 0.55f * A), 0.f, Canvas->ClipY * 0.4f, Canvas->ClipX, 120.f * S);
		DrawTextAt(TEXT("MISSION COMPLETE"), CX, Canvas->ClipY * 0.4f + 16.f * S, FLinearColor(Accent.R, Accent.G, Accent.B, A),
			GEngine->GetLargeFont(), 1.6f * S, true);
		DrawTextAt(FString::Printf(TEXT("Course set for planet %s"), *Mission::PlanetName(Droid->GetPlanet()->GetPlanetNumber() + 1)),
			CX, Canvas->ClipY * 0.4f + 70.f * S, FLinearColor(Text.R, Text.G, Text.B, A), Medium, 1.1f * S, true);
	}

	if (Droid->IsKnockedOut())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawTextAt(TEXT("SYSTEMS OFFLINE"), CX, Canvas->ClipY * 0.42f, FLinearColor(1.f, 0.3f, 0.25f), GEngine->GetLargeFont(), 1.6f * S, true);
		DrawTextAt(TEXT("Rebooting at the ship..."), CX, Canvas->ClipY * 0.42f + 56.f * S, Text, Medium, 1.1f * S, true);
	}

	// crosshair where blaster shots go
	if (Droid->GetBlasterTime() > 0.f && !Droid->IsKnockedOut())
	{
		const FVector Aim = Project(Droid->GetMuzzleLocation() + Droid->GetAimDirection() * 1500.f);
		if (Aim.Z > 0.f)
		{
			const FLinearColor C(1.f, 0.6f, 0.2f, 0.9f);
			const float K = 10.f * S, G = 4.f * S;
			DrawLine(Aim.X - K - G, Aim.Y, Aim.X - G, Aim.Y, C, 2.f);
			DrawLine(Aim.X + G, Aim.Y, Aim.X + K + G, Aim.Y, C, 2.f);
			DrawLine(Aim.X, Aim.Y - K - G, Aim.X, Aim.Y - G, C, 2.f);
			DrawLine(Aim.X, Aim.Y + G, Aim.X, Aim.Y + K + G, C, 2.f);
		}
	}

	DrawTextAt(TEXT("WASD roll   Mouse look   Shift boost   Ctrl careful   Space jump   E interact   LMB fire   Wheel zoom"),
		Canvas->ClipX - 24.f * S - 760.f * S, Canvas->ClipY - 36.f * S, FLinearColor(1.f, 1.f, 1.f, 0.45f), GEngine->GetSmallFont(), 0.95f * S);
}

void AMissionHUD::DrawVitals(ARollerDroid* Droid)
{
	const float X = 24.f * S, Y = 24.f * S + 118.f * S + 10.f * S;
	const float W = 520.f * S, H = 76.f * S;
	DrawPanel(X, Y, W, H);
	const float Frac = Droid->GetHealth() / FMath::Max(Droid->GetMaxHealth(), 1.f);
	const FLinearColor Bar = Frac > 0.5f ? FLinearColor(0.3f, 1.f, 0.55f) : Frac > 0.25f ? FLinearColor(1.f, 0.8f, 0.2f)
		: FLinearColor(1.f, 0.25f, 0.2f);
	DrawTextAt(TEXT("HP"), X + 16.f * S, Y + 10.f * S, Dim, GEngine->GetSmallFont(), 1.1f * S);
	const float BX = X + 56.f * S, BY = Y + 12.f * S, BW = 330.f * S, BH = 16.f * S;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), BX, BY, BW, BH);
	DrawRect(Bar, BX, BY, BW * Frac, BH);
	if (Droid->GetShieldTime() > 0.f) // shield overlay on the bar
	{
		DrawRect(FLinearColor(0.3f, 0.6f, 1.f, 0.45f), BX, BY - 3.f * S, BW, 3.f * S);
		DrawRect(FLinearColor(0.3f, 0.6f, 1.f, 0.45f), BX, BY + BH, BW, 3.f * S);
	}
	DrawTextAt(FString::Printf(TEXT("%.0f / %.0f"), Droid->GetHealth(), Droid->GetMaxHealth()), BX + BW + 14.f * S, Y + 10.f * S,
		Text, GEngine->GetSmallFont(), 1.1f * S);

	// active power-ups / modes
	float SX = X + 16.f * S;
	const float SY = Y + 42.f * S;
	auto Chip = [&](const FString& Label, const FLinearColor& Col)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Label, TW, TH, GEngine->GetSmallFont(), 1.0f * S);
		DrawRect(FLinearColor(Col.R, Col.G, Col.B, 0.25f), SX, SY, TW + 16.f * S, 24.f * S);
		DrawRect(Col, SX, SY, 3.f * S, 24.f * S);
		DrawTextAt(Label, SX + 9.f * S, SY + 3.f * S, Text, GEngine->GetSmallFont(), 1.0f * S);
		SX += TW + 26.f * S;
	};
	if (Droid->GetShieldTime() > 0.f)
	{
		Chip(FString::Printf(TEXT("SHIELD %.0fs"), FMath::CeilToFloat(Droid->GetShieldTime())), APowerUp::KindColor(EPowerUpKind::Shield));
	}
	if (Droid->GetBlasterTime() > 0.f)
	{
		Chip(FString::Printf(TEXT("BLASTER %.0fs"), FMath::CeilToFloat(Droid->GetBlasterTime())), APowerUp::KindColor(EPowerUpKind::Blaster));
	}
	if (Droid->IsCareful())
	{
		Chip(TEXT("CAREFUL"), FLinearColor(0.8f, 0.8f, 0.9f));
	}
	if (SX == X + 16.f * S)
	{
		DrawTextAt(TEXT("No power-ups. Green + repairs, blue shields, orange blaster."), SX, SY + 3.f * S, Dim, GEngine->GetSmallFont(), 1.0f * S);
	}
}
