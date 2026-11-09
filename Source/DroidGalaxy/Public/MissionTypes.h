#pragma once

#include "CoreMinimal.h"

/** Static mission data shared by the planet, the ship, the droid and the HUD. */
namespace Mission
{
	constexpr int32 NumParts = 5;

	struct FPartInfo
	{
		const TCHAR* Name;
		FLinearColor Color;
		/** Where the part sits on the ship once installed (ship-local cm; from SourceAssets/prepared/ship_sockets.json). */
		FVector SocketLocation;
		float SocketYaw;
	};

	inline const FPartInfo& Part(int32 Index)
	{
		static const FPartInfo Parts[NumParts] = {
			{TEXT("Power Core"), FLinearColor(0.2f, 1.f, 0.95f), FVector(0.f, -127.4f, 205.f), -90.f},
			{TEXT("Nav Computer"), FLinearColor(0.55f, 1.f, 0.2f), FVector(-110.f, 63.5f, 270.f), -210.f},
			{TEXT("Thruster Nozzle"), FLinearColor(1.f, 0.45f, 0.1f), FVector(103.9f, 60.f, 145.f), -330.f},
			{TEXT("Fuel Cell"), FLinearColor(1.f, 0.85f, 0.2f), FVector(-106.2f, -61.3f, 155.f), -150.f},
			{TEXT("Hyperdrive Crystal"), FLinearColor(0.7f, 0.35f, 1.f), FVector(0.f, 0.f, 555.f), 0.f},
		};
		return Parts[FMath::Clamp(Index, 0, NumParts - 1)];
	}

	inline FString PlanetName(int32 Number)
	{
		static const TCHAR* Names[] = {TEXT("Veyra"), TEXT("Okkar"), TEXT("Thessil"), TEXT("Brumo"), TEXT("Ixa")};
		const int32 N = FMath::Max(Number, 1) - 1;
		const FString Base = Names[N % UE_ARRAY_COUNT(Names)];
		return N < UE_ARRAY_COUNT(Names) ? Base : FString::Printf(TEXT("%s %d"), *Base, N / UE_ARRAY_COUNT(Names) + 1);
	}
}
