#pragma once

#include "CoreMinimal.h"

/**
 * Street grid of a square city centred on the world origin. It is the single source of truth for
 * streets and blocks: a block is exactly the gap between four crossings, so blocks are derived from
 * the streets and can never overlap them.
 *
 *   Cell          = BlockSize + StreetWidth
 *   BlocksPerSide = Floor((CitySize - StreetWidth) / Cell), between 1 and MaxBlocksPerSide
 *
 * The inputs are in metres; everything stored here is in centimetres (Unreal units).
 */
struct EGOITZPINEIROTOOLSRUNTIME_API FCityGrid
{
	static constexpr int32 MaxBlocksPerSide = 150;

	int32 BlocksPerSide = 1;
	double StreetWidth = 800.0;
	double BlockSize = 6000.0;
	double Cell = 6800.0;

	/** True if the parameters asked for less than one block or more than MaxBlocksPerSide per side. */
	bool bClamped = false;

	static FCityGrid Make(double CitySizeMeters, double BlockSizeMeters, double StreetWidthMeters = 8.0);

	int32 CrossingsPerSide() const { return BlocksPerSide + 1; }

	/** Side of the whole city, outer streets included (cm). */
	double Extent() const { return BlocksPerSide * Cell + StreetWidth; }

	/** X (column) or Y (row) of the crossings with that index (cm). Index goes from 0 to BlocksPerSide. */
	double CrossingCoord(int32 Index) const { return (Index - 0.5 * BlocksPerSide) * Cell; }

	/** X (column) or Y (row) of the centre of the blocks with that index (cm). Index goes from 0 to BlocksPerSide - 1. */
	double BlockCoord(int32 Index) const { return CrossingCoord(Index) + 0.5 * Cell; }
};
