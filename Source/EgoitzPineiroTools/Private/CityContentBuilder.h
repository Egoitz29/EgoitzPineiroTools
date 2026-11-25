#pragma once

#include "CoreMinimal.h"

class UPackage;

/**
 * Creates the content the city needs inside the plugin (Content/City): materials, street furniture
 * meshes and PCG graphs. Assets that already exist are kept as they are, so it can be run again
 * safely. The exception is a graph that is incomplete (an earlier run could not wire it): it is emptied
 * and built again. To rebuild any other asset, delete it in the Content Browser and run the builder again.
 */
class FCityContentBuilder
{
public:
	/** What happened in one run. Errors also go to the Output Log (LogCityContent). */
	struct FReport
	{
		TArray<FString> Created;
		TArray<FString> Rebuilt;
		TArray<FString> Skipped;
		TArray<FString> Errors;

		FText ToText() const;
	};

	static FReport Build();

	/** Root of the generated content and its sub-folders (long package paths). */
	static const TCHAR* RootPath;
	static const TCHAR* MaterialsPath;
	static const TCHAR* MeshesPath;
	static const TCHAR* SubgraphsPath;

private:
	explicit FCityContentBuilder(FReport& InReport) : Report(InReport) {}

	void BuildMaterials();
	void BuildMeshes();
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
