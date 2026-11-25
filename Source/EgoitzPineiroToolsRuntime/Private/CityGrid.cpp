#include "CityGrid.h"

FCityGrid FCityGrid::Make(double CitySizeMeters, double BlockSizeMeters, double StreetWidthMeters)
{
	FCityGrid Grid;
	Grid.StreetWidth = FMath::Max(StreetWidthMeters, 1.0) * 100.0;
	Grid.BlockSize = FMath::Max(BlockSizeMeters, 5.0) * 100.0;
	Grid.Cell = Grid.BlockSize + Grid.StreetWidth;

	const int32 Wanted = FMath::FloorToInt32((FMath::Max(CitySizeMeters, 0.0) * 100.0 - Grid.StreetWidth) / Grid.Cell);
	Grid.BlocksPerSide = FMath::Clamp(Wanted, 1, MaxBlocksPerSide);
	Grid.bClamped = Grid.BlocksPerSide != Wanted;
	return Grid;
}
