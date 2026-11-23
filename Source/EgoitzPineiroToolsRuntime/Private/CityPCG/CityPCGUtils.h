#pragma once

#include "CoreMinimal.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

struct FPCGContext;
class UPCGData;

/**
 * Helpers shared by the city nodes.
 *
 * Every layer of the city is a set of points read as boxes: the point transform carries the
 * position, the yaw and the size (as scale), and the bounds are a 1 m cube (+-50 cm). That is
 * exactly the engine cube (/Engine/BasicShapes/Cube), so a Static Mesh Spawner with that mesh
 * draws each point at its real size, and Self Pruning or Distance see the real boxes.
 */
namespace CityPCG
{
	/** A point seen as a box. Sizes in cm, local to the point (before the yaw). */
	struct FCityBox
	{
		FVector Center = FVector::ZeroVector;
		FVector Size = FVector::ZeroVector;
		double Yaw = 0.0;
		int32 Seed = 0;

		double Top() const { return Center.Z + 0.5 * Size.Z; }
		double Bottom() const { return Center.Z - 0.5 * Size.Z; }
	};

	/** Reads every point connected to a pin as a box, in input order. Non-point data is ignored. */
	TArray<FCityBox> ReadBoxes(const FPCGContext* Context, FName PinLabel);

	/** Transform that turns a +-50 cm point (the engine cube) into a box of the given size and yaw. */
	FTransform MakeBoxTransform(const FVector& Center, const FVector& Size, double YawDegrees = 0.0);

	/**
	 * New point data with NumPoints points. Transform, seed and metadata entry are allocated and
	 * every point gets its own metadata entry, ready for attributes. All points share the same
	 * local bounds: +-Extents (50 cm by default, the engine cube).
	 */
	UPCGBasePointData* NewPoints(FPCGContext* Context, int32 NumPoints, const FVector& Extents = FVector(50.0));

	/** Creates a per-point attribute on the point data. */
	template <typename T>
	FPCGMetadataAttribute<T>* CreateAttribute(UPCGBasePointData* Points, FName Name, const T& DefaultValue)
	{
		return Points->MutableMetadata()->CreateAttribute<T>(Name, DefaultValue, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false);
	}

	/** Sends a data object to an output pin. */
	void AddOutput(FPCGContext* Context, UPCGData* Data, FName PinLabel);

	/**
	 * A street grid recovered from a set of crossings: the distinct X and Y coordinates, sorted,
	 * give the column and row of each crossing. This is how blocks and streets derive from the
	 * crossings instead of recomputing the grid from the parameters.
	 */
	struct FCrossingGrid
	{
		TArray<double> Xs;                  // distinct crossing X, ascending (one per column)
		TArray<double> Ys;                  // distinct crossing Y, ascending (one per row)
		TArray<FIntPoint> Cells;            // (Col, Row) of each crossing, same order as the input
		TMap<FIntPoint, int32> CellToIndex; // crossing index at (Col, Row)

		int32 Find(int32 Col, int32 Row) const
		{
			const int32* Index = CellToIndex.Find(FIntPoint(Col, Row));
			return Index ? *Index : INDEX_NONE;
		}
	};

	FCrossingGrid BuildCrossingGrid(const TArray<FCityBox>& Crossings);
}
