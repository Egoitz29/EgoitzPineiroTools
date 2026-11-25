#pragma once

#include "CoreMinimal.h"

/**
 * Pin and attribute names shared by the city nodes, the graphs the editor builds and the docs.
 * In English and without spaces, so they can be typed as attribute selectors in any PCG node.
 */
namespace CityPCGNames
{
	// Pins
	inline const FName RoadsPin = TEXT("Roads");
	inline const FName BlocksPin = TEXT("Blocks");
	inline const FName LotsPin = TEXT("Lots");
	inline const FName BuildingsPin = TEXT("Buildings");
	inline const FName PointsPin = TEXT("Points");

	// Attributes
	inline const FName RoadKind = TEXT("RoadKind"); // int32: 0 = crossing, 1 = street between two crossings
	inline const FName Axis = TEXT("Axis");         // int32: street along X (0) or along Y (1); -1 in crossings
	inline const FName Col = TEXT("Col");           // int32: column in the street grid
	inline const FName Row = TEXT("Row");           // int32: row in the street grid
	inline const FName BlockID = TEXT("BlockID");   // int32: block a point belongs to
	inline const FName Side = TEXT("Side");         // int32: side of the block (0 south, 1 east, 2 north, 3 west)
	inline const FName Height = TEXT("Height");     // double: building height in metres
	inline const FName Floors = TEXT("Floors");     // int32: building floors (3.5 m each)
}
