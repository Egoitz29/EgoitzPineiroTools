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

/** What the panel shows about the city currently in the level. */
struct FCityStats
{
	int32 NumCities = 0;
	bool bGenerating = false;
	int32 TotalInstances = 0;
	/** "Mesh / Material" and how many instances of it, sorted by name. */
	TArray<TPair<FString, int32>> InstancesPerType;
	/**
	 * Order-independent hash of every generated instance (mesh, material, position, rotation, scale).
	 * Same seed and parameters -> same fingerprint. Used to show the determinism in the demo.
	 */
	uint32 Fingerprint = 0;
};

/**
 * Everything the panel does, without any UI: create the city actor, push the parameters into
 * PCG_City, generate, clean up and measure the result.
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

	/** Counts the generated instances and computes the fingerprint of the city in the level. */
	FCityStats ComputeStats() const;

	/** True if PCG_City exists in the plugin's Content folder. */
	static bool IsGraphAvailable();

private:
	static UWorld* GetEditorWorld();
	static TArray<ACityGeneratorActor*> FindCities(UWorld* World);
	void ClampParams();
};
