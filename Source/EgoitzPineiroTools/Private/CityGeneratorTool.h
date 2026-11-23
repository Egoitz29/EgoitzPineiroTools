#pragma once

#include "CoreMinimal.h"

class ACityGeneratorActor;
class UWorld;

/** The six values the scenario engineers control from the panel. Distances in metres. */
struct FCityGenerationParams
{
	double CitySize = 400.0;
	double BlockSize = 60.0;
	double BuildingDensity = 0.6;
	double BuildingHeight = 30.0;
	double BuildingVariation = 0.5;
	int32 Seed = 1234;
};

/**
 * Everything the panel does, without any UI: create the city actor, push the parameters into
 * PCG_City, generate and clean up.
 */
class FCityGeneratorTool
{
public:
	/** Main graph of the city, inside the plugin's Content folder. */
	static const TCHAR* GraphPackage;
	static const TCHAR* GraphObjectPath;

	/** Names of the Graph Parameters of PCG_City. They must exist there, with type Double. */
	static const FName CitySizeParam;
	static const FName BlockSizeParam;
	static const FName BuildingDensityParam;
	static const FName BuildingHeightParam;
	static const FName BuildingVariationParam;

	FCityGenerationParams Params;

	/**
	 * Generates the city with the current parameters.
	 * bFullRebuild = true deletes any previous city actor and creates a new one (the Generate button).
	 * bFullRebuild = false reuses the current city actor if there is one (live updates from the panel).
	 */
	bool Generate(bool bFullRebuild, FText& OutMessage);

	/** Removes every actor the tool created (tag EPT_CityGenerated) with all its generated content. Returns how many. */
	int32 Cleanup();

	bool HasCity() const;

	/** True if PCG_City exists in the plugin's Content folder. */
	static bool IsGraphAvailable();

private:
	static UWorld* GetEditorWorld();
	static TArray<ACityGeneratorActor*> FindCities(UWorld* World);
	void ClampParams();
};
