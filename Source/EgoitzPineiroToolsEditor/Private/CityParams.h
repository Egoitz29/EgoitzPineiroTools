#pragma once

#include "CoreMinimal.h"

/** The values the scenario engineers control from the panel. Distances in metres. */
struct FCityParams
{
	double CitySize = 400.0;
	double BlockSize = 60.0;
	double BuildingDensity = 0.6;
	double BuildingHeight = 30.0;
	double BuildingVariation = 0.5;
	int32 Seed = 1234;

	// Parks
	double ParkRatio = 0.15;
	double ParkTreeDensity = 0.6;
	double ParkTreeScaleVariation = 0.4;

	// Urban zoning: a dense, tall centre and a low, scattered periphery
	bool bZoning = true;
	double ZoneRadius = 120.0;
	double ZoneTransition = 0.5;
	double ZoneCenterHeight = 2.0;
	double ZonePeripheryHeight = 0.5;
	double ZoneCenterDensity = 1.4;
	double ZonePeripheryDensity = 0.6;

	/** Keeps every value inside the range the panel offers. */
	void Clamp()
	{
		CitySize = FMath::Clamp(CitySize, 50.0, 3000.0);
		BlockSize = FMath::Clamp(BlockSize, 15.0, 300.0);
		BuildingDensity = FMath::Clamp(BuildingDensity, 0.0, 1.0);
		BuildingHeight = FMath::Clamp(BuildingHeight, 3.0, 300.0);
		BuildingVariation = FMath::Clamp(BuildingVariation, 0.0, 1.0);
		Seed = FMath::Clamp(Seed, 0, 9999999);
		ParkRatio = FMath::Clamp(ParkRatio, 0.0, 1.0);
		ParkTreeDensity = FMath::Clamp(ParkTreeDensity, 0.0, 1.0);
		ParkTreeScaleVariation = FMath::Clamp(ParkTreeScaleVariation, 0.0, 1.0);
		ZoneRadius = FMath::Clamp(ZoneRadius, 10.0, 3000.0);
		ZoneTransition = FMath::Clamp(ZoneTransition, 0.0, 1.0);
		ZoneCenterHeight = FMath::Clamp(ZoneCenterHeight, 0.1, 5.0);
		ZonePeripheryHeight = FMath::Clamp(ZonePeripheryHeight, 0.1, 5.0);
		ZoneCenterDensity = FMath::Clamp(ZoneCenterDensity, 0.0, 2.0);
		ZonePeripheryDensity = FMath::Clamp(ZonePeripheryDensity, 0.0, 2.0);
	}
};

/**
 * Graph Parameters of the city graphs (type Double). The panel writes them into PCG_City, PCG_City
 * passes them down to the subgraphs that use them, and each subgraph connects them to the property
 * of its node (City Street Grid, City Lots, City Buildings, City Park). With zoning off, the panel
 * writes 1 in the four zone factors, which leaves height and density as they are everywhere.
 */
namespace CityParamNames
{
	inline const FName CitySize = TEXT("CitySize");
	inline const FName BlockSize = TEXT("BlockSize");
	inline const FName BuildingDensity = TEXT("BuildingDensity");
	inline const FName BuildingHeight = TEXT("BuildingHeight");
	inline const FName BuildingVariation = TEXT("BuildingVariation");
	inline const FName ParkRatio = TEXT("ParkRatio");
	inline const FName ParkTreeDensity = TEXT("ParkTreeDensity");
	inline const FName ParkTreeScaleVariation = TEXT("ParkTreeScaleVariation");
	inline const FName ZoneRadius = TEXT("ZoneRadius");
	inline const FName ZoneTransition = TEXT("ZoneTransition");
	inline const FName ZoneCenterHeight = TEXT("ZoneCenterHeight");
	inline const FName ZonePeripheryHeight = TEXT("ZonePeripheryHeight");
	inline const FName ZoneCenterDensity = TEXT("ZoneCenterDensity");
	inline const FName ZonePeripheryDensity = TEXT("ZonePeripheryDensity");

	/** Only in PCG_City: version of the code that built the graphs (see FCityContent::GraphVersion). */
	inline const FName GraphVersion = TEXT("GraphVersion");
}
