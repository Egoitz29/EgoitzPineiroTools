#pragma once

#include "CityContent.h"
#include "CityParams.h"
#include "CoreMinimal.h"

class AProceduralCityActor;
class UWorld;

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
	 * Same seed and parameters -> same fingerprint. Shows the determinism in the demo.
	 */
	uint32 Fingerprint = 0;
};

/**
 * Everything the panel does, without any UI: make sure the content exists, create the city actor,
 * push the parameters into PCG_City, generate, clean up and measure the result.
 */
class FCityGenerator
{
public:
	FCityParams Params;

	/** What the last content check did (assets created, graphs rebuilt, errors). */
	FCityContent::FReport LastContentReport;

	/**
	 * Generates the city with the current parameters.
	 * bFullRebuild = true removes any previous city and creates a new one (the Generate button).
	 * bFullRebuild = false reuses the city in the level if there is one (live updates from the panel).
	 */
	bool Generate(bool bFullRebuild, FText& OutMessage);

	/** Removes every actor the tool created (tag EPT_CityGenerated) with all its generated content. Returns how many. */
	int32 Cleanup();

	bool HasCity() const;

	/** Counts the generated instances and computes the fingerprint of the city in the level. */
	FCityStats ComputeStats() const;

private:
	static UWorld* GetEditorWorld();
	static TArray<AProceduralCityActor*> FindCities(UWorld* World);
};
