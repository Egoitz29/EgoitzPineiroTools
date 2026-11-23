#include "CityPCG/PCGCityBlocks.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityBlocks"

#if WITH_EDITOR
FText UPCGCityBlocksSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "One raised block in each cell of four crossings, filling exactly the space between the roads.");
}
#endif

TArray<FPCGPinProperties> UPCGCityBlocksSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::CrossingsPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCityBlocksSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityBlocksSettings::CreateElement() const
{
	return MakeShared<FPCGCityBlocksElement>();
}

bool FPCGCityBlocksElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityBlocksElement::Execute);

	const UPCGCityBlocksSettings* Settings = Context->GetInputSettings<UPCGCityBlocksSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Crossings = CityPCG::ReadBoxes(Context, CityPCGNames::CrossingsPin);
	if (Crossings.Num() < 4)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoCrossings", "City Blocks needs a grid of at least 2 x 2 crossings on the Crossings pin."));
		return true;
	}

	const CityPCG::FCrossingGrid Grid = CityPCG::BuildCrossingGrid(Crossings);
	const double SidewalkHeight = FMath::Max(Settings->SidewalkHeight, 0.01) * 100.0;
	const int32 BlocksPerRow = FMath::Max(Grid.Xs.Num() - 1, 1);

	struct FBlock
	{
		FVector Center;
		FVector Size;
		int32 Col;
		int32 Row;
	};
	TArray<FBlock> Blocks;

	// Row by row, column by column: the order (and so BlockID) never depends on the input order
	for (int32 Row = 0; Row + 1 < Grid.Ys.Num(); ++Row)
	{
		for (int32 Col = 0; Col + 1 < Grid.Xs.Num(); ++Col)
		{
			const int32 SW = Grid.Find(Col, Row);
			const int32 SE = Grid.Find(Col + 1, Row);
			const int32 NW = Grid.Find(Col, Row + 1);
			const int32 NE = Grid.Find(Col + 1, Row + 1);
			if (SW == INDEX_NONE || SE == INDEX_NONE || NW == INDEX_NONE || NE == INDEX_NONE)
			{
				continue; // not a closed cell: no block
			}

			// The block ends exactly where the road boxes of its corners end
			const double MinX = Crossings[SW].Center.X + 0.5 * Crossings[SW].Size.X;
			const double MaxX = Crossings[SE].Center.X - 0.5 * Crossings[SE].Size.X;
			const double MinY = Crossings[SW].Center.Y + 0.5 * Crossings[SW].Size.Y;
			const double MaxY = Crossings[NW].Center.Y - 0.5 * Crossings[NW].Size.Y;
			if (MaxX - MinX < 100.0 || MaxY - MinY < 100.0)
			{
				continue; // less than 1 m: nothing fits
			}

			const double Ground = Crossings[SW].Bottom();
			const FVector Center(0.5 * (MinX + MaxX), 0.5 * (MinY + MaxY), Ground + 0.5 * SidewalkHeight);
			Blocks.Add({ Center, FVector(MaxX - MinX, MaxY - MinY, SidewalkHeight), Col, Row });
		}
	}

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Blocks.Num());
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
	FPCGMetadataAttribute<int32>* ColAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Col, 0);
	FPCGMetadataAttribute<int32>* RowAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Row, 0);

	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		const FBlock& Block = Blocks[Index];
		Transforms[Index] = CityPCG::MakeBoxTransform(Block.Center, Block.Size);
		Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Block.Center);
		BlockIDAttribute->SetValue(Entries[Index], Block.Row * BlocksPerRow + Block.Col);
		ColAttribute->SetValue(Entries[Index], Block.Col);
		RowAttribute->SetValue(Entries[Index], Block.Row);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::BlocksPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
