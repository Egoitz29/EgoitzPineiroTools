#pragma once

#include "CoreMinimal.h"

/** The six values the scenario engineers control from the panel. Distances in metres. */
struct FCityParams
{
	double CitySize = 400.0;
	double BlockSize = 60.0;
	double BuildingDensity = 0.6;
	double BuildingHeight = 30.0;
	double BuildingVariation = 0.5;
	int32 Seed = 1234;

	/** Keeps every value inside the range the panel offers. */
	void Clamp()
	{
		CitySize = FMath::Clamp(CitySize, 50.0, 3000.0);
		BlockSize = FMath::Clamp(BlockSize, 15.0, 300.0);
		BuildingDensity = FMath::Clamp(BuildingDensity, 0.0, 1.0);
		BuildingHeight = FMath::Clamp(BuildingHeight, 3.0, 300.0);
		BuildingVariation = FMath::Clamp(BuildingVariation, 0.0, 1.0);
		Seed = FMath::Clamp(Seed, 0, 9999999);
	}
};

/**
 * Graph Parameters of the city graphs (type Double). The panel writes the first five into PCG_City,
 * PCG_City passes them down to its subgraphs, and the subgraphs connect them to the node properties
 * with the same name (City Street Grid, City Lots, City Buildings).
 */
namespace CityParamNames
{
	inline const FName CitySize = TEXT("CitySize");
	inline const FName BlockSize = TEXT("BlockSize");
	inline const FName BuildingDensity = TEXT("BuildingDensity");
	inline const FName BuildingHeight = TEXT("BuildingHeight");
	inline const FName BuildingVariation = TEXT("BuildingVariation");

	/** Only in PCG_City: version of the code that built the graphs (see FCityContent::GraphVersion). */
	inline const FName GraphVersion = TEXT("GraphVersion");
}
