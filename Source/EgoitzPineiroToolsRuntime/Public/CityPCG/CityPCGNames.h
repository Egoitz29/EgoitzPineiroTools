#pragma once

#include "CoreMinimal.h"

/**
 * Pin and attribute names shared by the city nodes, the PCG graphs and the documentation.
 * In English and without spaces, so they can be typed as selectors in any node (Col, $Density...).
 */
namespace CityPCGNames
{
	// Pins
	inline const FName CrossingsPin = TEXT("Crossings");
	inline const FName SegmentsPin = TEXT("Segments");
	inline const FName BlocksPin = TEXT("Blocks");
	inline const FName LotsPin = TEXT("Lots");
	inline const FName BuildingsPin = TEXT("Buildings");
	inline const FName SlotsPin = TEXT("Slots");

	// Attributes
	inline const FName Col = TEXT("Col");             // int32: column of a crossing or block in the street grid
	inline const FName Row = TEXT("Row");             // int32: row of a crossing or block in the street grid
	inline const FName Axis = TEXT("Axis");           // int32: 0 = street along X, 1 = street along Y
	inline const FName BlockID = TEXT("BlockID");     // int32: block a lot or a sidewalk slot belongs to
	inline const FName Frontage = TEXT("Frontage");   // bool: the lot touches the street (false = courtyard)
	inline const FName Height = TEXT("Height");       // double: building height in metres
	inline const FName Floors = TEXT("Floors");       // int32: building floors (3.5 m each)
	inline const FName PropType = TEXT("PropType");   // int32: which street furniture goes in a sidewalk slot
	inline const FName Side = TEXT("Side");           // int32: side of the block (0 S, 1 E, 2 N, 3 W)
}
