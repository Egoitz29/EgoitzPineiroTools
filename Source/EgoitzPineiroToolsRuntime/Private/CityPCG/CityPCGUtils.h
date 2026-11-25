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
 * Streets, blocks, lots and buildings are points read as boxes: the point transform carries the
 * position, the yaw and the size (as scale), and the bounds are a 1 m cube (+-50 cm). That is
 * exactly the engine cube (/Engine/BasicShapes/Cube), so a Static Mesh Spawner with that mesh
 * draws each point at its real size, and Self Pruning sees the real boxes.
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
		double MinX() const { return Center.X - 0.5 * Size.X; }
		double MaxX() const { return Center.X + 0.5 * Size.X; }
		double MinY() const { return Center.Y - 0.5 * Size.Y; }
		double MaxY() const { return Center.Y + 0.5 * Size.Y; }
	};

	/** Reads every point connected to a pin as a box, in input order. Data that is not points is ignored. */
	TArray<FCityBox> ReadBoxes(const FPCGContext* Context, FName PinLabel);

	/** Transform that turns a +-50 cm point (the engine cube) into a box of the given size and yaw. */
	FTransform MakeBoxTransform(const FVector& Center, const FVector& Size, double YawDegrees = 0.0);

	/**
	 * New point data with NumPoints points. Transform, seed and metadata entry are allocated (plus
	 * ExtraProperties), and every point gets its own metadata entry, ready for attributes. All points
	 * share the same local bounds: +-Extents (50 cm by default, the engine cube).
	 */
	UPCGBasePointData* NewPoints(FPCGContext* Context, int32 NumPoints, const FVector& Extents = FVector(50.0),
		EPCGPointNativeProperties ExtraProperties = EPCGPointNativeProperties::None);

	/** Creates a per-point attribute on the point data. */
	template <typename T>
	FPCGMetadataAttribute<T>* CreateAttribute(UPCGBasePointData* Points, FName Name, const T& DefaultValue)
	{
		return Points->MutableMetadata()->CreateAttribute<T>(Name, DefaultValue, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false);
	}

	/** Sends a data object to an output pin. */
	void AddOutput(FPCGContext* Context, UPCGData* Data, FName PinLabel);
}
