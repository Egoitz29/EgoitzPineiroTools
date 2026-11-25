#include "CityPCG/PCGCityStreetGrid.h"

#include "CityGrid.h"
#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Math/RandomStream.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityStreetGrid"

#if WITH_EDITOR
FText UPCGCityStreetGridSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Street grid centred on the origin: road boxes (crossings and streets) and the blocks between them, split into urban blocks and parks.");
}
#endif

TArray<FPCGPinProperties> UPCGCityStreetGridSettings::InputPinProperties() const
{
	return {}; // a generator: it only needs its parameters
}

TArray<FPCGPinProperties> UPCGCityStreetGridSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::RoadsPin, EPCGDataType::Point);
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	Pins.Emplace(CityPCGNames::UrbanBlocksPin, EPCGDataType::Point);
	Pins.Emplace(CityPCGNames::ParkBlocksPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityStreetGridSettings::CreateElement() const
{
	return MakeShared<FPCGCityStreetGridElement>();
}

bool FPCGCityStreetGridElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityStreetGridElement::Execute);

	const UPCGCityStreetGridSettings* Settings = Context->GetInputSettings<UPCGCityStreetGridSettings>();
	check(Settings);

	const FCityGrid Grid = FCityGrid::Make(Settings->CitySize, Settings->BlockSize, Settings->StreetWidth);
	if (Grid.bClamped)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("Clamped", "CitySize and BlockSize ask for less than 1 or more than {0} blocks per side: clamped."), FCityGrid::MaxBlocksPerSide));
	}

	const int32 N = Grid.BlocksPerSide;
	const double Street = Grid.StreetWidth;
	const double Thickness = FMath::Max(Settings->RoadThickness, 0.01) * 100.0;
	const double SidewalkHeight = FMath::Max(Settings->SidewalkHeight, 0.01) * 100.0;

	// --- Roads: (N+1)^2 crossings and 2 * N * (N+1) streets between them ---------------------
	{
		const int32 NumRoads = (N + 1) * (N + 1) + 2 * N * (N + 1);
		UPCGBasePointData* Roads = CityPCG::NewPoints(Context, NumRoads);
		auto Transforms = Roads->GetTransformValueRange(/*bAllocate=*/false);
		auto Seeds = Roads->GetSeedValueRange(/*bAllocate=*/false);
		auto Entries = Roads->GetMetadataEntryValueRange(/*bAllocate=*/false);
		FPCGMetadataAttribute<int32>* KindAttribute = CityPCG::CreateAttribute<int32>(Roads, CityPCGNames::RoadKind, 0);
		FPCGMetadataAttribute<int32>* AxisAttribute = CityPCG::CreateAttribute<int32>(Roads, CityPCGNames::Axis, -1);
		FPCGMetadataAttribute<int32>* ColAttribute = CityPCG::CreateAttribute<int32>(Roads, CityPCGNames::Col, 0);
		FPCGMetadataAttribute<int32>* RowAttribute = CityPCG::CreateAttribute<int32>(Roads, CityPCGNames::Row, 0);

		int32 Index = 0;
		auto AddRoad = [&](double X, double Y, double SizeX, double SizeY, int32 Kind, int32 Axis, int32 Col, int32 Row)
		{
			const FVector Center(X, Y, 0.5 * Thickness); // the slab stands on Z = 0
			Transforms[Index] = CityPCG::MakeBoxTransform(Center, FVector(SizeX, SizeY, Thickness));
			Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Center);
			KindAttribute->SetValue(Entries[Index], Kind);
			AxisAttribute->SetValue(Entries[Index], Axis);
			ColAttribute->SetValue(Entries[Index], Col);
			RowAttribute->SetValue(Entries[Index], Row);
			++Index;
		};

		for (int32 Row = 0; Row <= N; ++Row)
		{
			for (int32 Col = 0; Col <= N; ++Col)
			{
				AddRoad(Grid.CrossingCoord(Col), Grid.CrossingCoord(Row), Street, Street, 0, -1, Col, Row);
			}
		}
		// Streets along X: between crossing (Col, Row) and (Col + 1, Row). They end where the crossings start.
		for (int32 Row = 0; Row <= N; ++Row)
		{
			for (int32 Col = 0; Col < N; ++Col)
			{
				AddRoad(Grid.BlockCoord(Col), Grid.CrossingCoord(Row), Grid.BlockSize, Street, 1, 0, Col, Row);
			}
		}
		// Streets along Y: between crossing (Col, Row) and (Col, Row + 1)
		for (int32 Col = 0; Col <= N; ++Col)
		{
			for (int32 Row = 0; Row < N; ++Row)
			{
				AddRoad(Grid.CrossingCoord(Col), Grid.BlockCoord(Row), Street, Grid.BlockSize, 1, 1, Col, Row);
			}
		}

		CityPCG::AddOutput(Context, Roads, CityPCGNames::RoadsPin);
	}

	// --- Blocks: the N x N gaps between the streets, and which of them are parks ----------------
	struct FBlock
	{
		FVector Center;
		int32 BlockID;
		int32 Col;
		int32 Row;
		int32 Seed;
		bool bPark;
	};

	const double ParkRatio = FMath::Clamp(Settings->ParkRatio, 0.0, 1.0);
	const int32 GraphSeed = Context->GetSeed(); // node seed combined with the PCG Component seed

	TArray<FBlock> AllBlocks;
	TArray<FBlock> UrbanBlocks;
	TArray<FBlock> ParkBlocks;
	AllBlocks.Reserve(N * N);
	for (int32 Row = 0; Row < N; ++Row)
	{
		for (int32 Col = 0; Col < N; ++Col)
		{
			FBlock& Block = AllBlocks.AddDefaulted_GetRef();
			Block.Center = FVector(Grid.BlockCoord(Col), Grid.BlockCoord(Row), 0.5 * SidewalkHeight);
			Block.BlockID = AllBlocks.Num() - 1;
			Block.Col = Col;
			Block.Row = Row;
			Block.Seed = PCGHelpers::ComputeSeedFromPosition(Block.Center);

			// The block decides alone, from the seed and its position: the same in every subgraph
			FRandomStream Random(PCGHelpers::ComputeSeed(GraphSeed, Block.Seed));
			Block.bPark = Random.FRand() < ParkRatio;
			(Block.bPark ? ParkBlocks : UrbanBlocks).Add(Block);
		}
	}

	auto OutputBlocks = [&](const TArray<FBlock>& Blocks, FName PinLabel)
	{
		UPCGBasePointData* Points = CityPCG::NewPoints(Context, Blocks.Num());
		auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
		auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
		auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
		FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
		FPCGMetadataAttribute<int32>* ColAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Col, 0);
		FPCGMetadataAttribute<int32>* RowAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Row, 0);
		FPCGMetadataAttribute<int32>* ParkAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Park, 0);

		for (int32 Index = 0; Index < Blocks.Num(); ++Index)
		{
			const FBlock& Block = Blocks[Index];
			Transforms[Index] = CityPCG::MakeBoxTransform(Block.Center, FVector(Grid.BlockSize, Grid.BlockSize, SidewalkHeight));
			Seeds[Index] = Block.Seed;
			BlockIDAttribute->SetValue(Entries[Index], Block.BlockID);
			ColAttribute->SetValue(Entries[Index], Block.Col);
			RowAttribute->SetValue(Entries[Index], Block.Row);
			ParkAttribute->SetValue(Entries[Index], Block.bPark ? 1 : 0);
		}

		CityPCG::AddOutput(Context, Points, PinLabel);
	};

	OutputBlocks(AllBlocks, CityPCGNames::BlocksPin);
	OutputBlocks(UrbanBlocks, CityPCGNames::UrbanBlocksPin);
	OutputBlocks(ParkBlocks, CityPCGNames::ParkBlocksPin);

	return true;
}

#undef LOCTEXT_NAMESPACE
