#pragma once

#include "CoreMinimal.h"

class UPackage;

/**
 * Everything the city needs inside the plugin (Content/CityGenerator), created by code so nobody
 * has to make or edit an asset by hand: materials, street furniture meshes and the PCG graphs.
 *
 * - Materials and meshes are created when they are missing. Existing ones are kept.
 * - The graphs carry the version of the code that built them (Graph Parameter GraphVersion of
 *   PCG_City). If a graph is missing, or the version is not this one, all the graphs are rebuilt
 *   in place. The version is only written when the build had no errors, so a failed build is
 *   tried again the next time.
 */
class FCityContent
{
public:
	/** What one run did. Errors also go to the Output Log (LogCityContent). */
	struct FReport
	{
		TArray<FString> Created;
		TArray<FString> Rebuilt;
		TArray<FString> Errors;

		bool IsEmpty() const { return Created.IsEmpty() && Rebuilt.IsEmpty() && Errors.IsEmpty(); }
		FString ToString() const;
	};

	/** Version of the graphs this code builds. Increase it whenever BuildGraphs changes. */
	static constexpr int32 GraphVersion = 3;

	/** Root of the generated content (long package path) and the main graph inside it. */
	static const TCHAR* RootPath;
	static FString CityGraphObjectPath();

	/** Creates what is missing and rebuilds the graphs if they are outdated (or always, with bRebuildGraphs). */
	static FReport Ensure(bool bRebuildGraphs);

private:
	explicit FCityContent(FReport& InReport) : Report(InReport) {}

	void BuildMaterials();
	void BuildMeshes();
	bool AreGraphsUpToDate() const;
	void BuildGraphs();
	void SaveAll();

	/** Asset at PackageName if it exists (on disk or in memory), loaded. */
	template <typename T>
	T* FindExisting(const FString& PackageName) const;

	/** New empty asset in its own package, registered and marked dirty. */
	template <typename T>
	T* CreateAsset(const FString& PackageName);

	void Error(const FString& Message);

	FReport& Report;
	TArray<UPackage*> PackagesToSave;
};
