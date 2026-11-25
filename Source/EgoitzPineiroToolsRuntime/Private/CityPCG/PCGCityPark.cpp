#include "CityPCG/PCGCityPark.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Math/RandomStream.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityPark"

namespace
{
	struct FPlant
	{
		FVector Location;
		double Scale;
		int32 Seed;
		int32 BlockID;
	};
}

#if WITH_EDITOR
FText UPCGCityParkSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "A lawn inside each park block, and spots for trees and bushes spread over it (TreeDensity, ScaleVariation).");
}
#endif

TArray<FPCGPinProperties> UPCGCityParkSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCityParkSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::LawnsPin, EPCGDataType::Point);
	Pins.Emplace(CityPCGNames::PlantsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityParkSettings::CreateElement() const
{
	return MakeShared<FPCGCityParkElement>();
}

bool FPCGCityParkElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityParkElement::Execute);

	const UPCGCityParkSettings* Settings = Context->GetInputSettings<UPCGCityParkSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Blocks = CityPCG::ReadBoxes(Context, CityPCGNames::BlocksPin);

	const double Density = FMath::Clamp(Settings->TreeDensity, 0.0, 1.0);
	const double ScaleVariation = FMath::Clamp(Settings->ScaleVariation, 0.0, 1.0);
	const double Sidewalk = FMath::Max(Settings->SidewalkWidth, 0.0) * 100.0;
	const double Spacing = FMath::Max(Settings->PlantSpacing, 1.0) * 100.0;
	const double Margin = FMath::Max(Settings->EdgeMargin, 0.0) * 100.0;
	const double LawnHeight = FMath::Max(Settings->LawnHeight, 0.01) * 100.0;
	const int32 GraphSeed = Context->GetSeed(); // node seed combined with the PCG Component seed

	TArray<CityPCG::FCityBox> Lawns;
	TArray<FPlant> Plants;
	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const CityPCG::FCityBox& Block = Blocks[BlockIndex];
		const double X0 = Block.MinX() + Sidewalk;
		const double X1 = Block.MaxX() - Sidewalk;
		const double Y0 = Block.MinY() + Sidewalk;
		const double Y1 = Block.MaxY() - Sidewalk;
		if (X1 - X0 < 100.0 || Y1 - Y0 < 100.0)
		{
			continue; // no room for a lawn
		}

		CityPCG::FCityBox& Lawn = Lawns.AddDefaulted_GetRef();
		Lawn.Center = FVector(0.5 * (X0 + X1), 0.5 * (Y0 + Y1), Block.Top() + 0.5 * LawnHeight);
		Lawn.Size = FVector(X1 - X0, Y1 - Y0, LawnHeight);
		Lawn.Seed = Block.Seed;

		// Candidate spots: a grid centred on the lawn, inside the margin
		const double InnerX = X1 - X0 - 2.0 * Margin;
		const double InnerY = Y1 - Y0 - 2.0 * Margin;
		if (InnerX < 0.0 || InnerY < 0.0)
		{
			continue;
		}
		const int32 CountX = FMath::FloorToInt32(InnerX / Spacing) + 1;
		const int32 CountY = FMath::FloorToInt32(InnerY / Spacing) + 1;
		const double StartX = X0 + Margin + 0.5 * (InnerX - (CountX - 1) * Spacing);
		const double StartY = Y0 + Margin + 0.5 * (InnerY - (CountY - 1) * Spacing);
		const double MaxShift = 0.35 * Spacing;

		for (int32 IndexY = 0; IndexY < CountY; ++IndexY)
		{
			for (int32 IndexX = 0; IndexX < CountX; ++IndexX)
			{
				const FVector GridSpot(StartX + IndexX * Spacing, StartY + IndexY * Spacing, Lawn.Top());
				const int32 SpotSeed = PCGHelpers::ComputeSeedFromPosition(GridSpot);

				// One random stream per spot: whether it has a plant, where exactly and how big
				FRandomStream Random(PCGHelpers::ComputeSeed(GraphSeed, SpotSeed));
				if (Random.FRand() >= Density)
				{
					continue;
				}
				const double ShiftX = MaxShift * Random.FRandRange(-1.0f, 1.0f);
				const double ShiftY = MaxShift * Random.FRandRange(-1.0f, 1.0f);
				const double Scale = 1.0 + ScaleVariation * Random.FRandRange(-0.5f, 0.5f);

				const FVector Location(
					FMath::Clamp(GridSpot.X + ShiftX, X0 + Margin, X1 - Margin),
					FMath::Clamp(GridSpot.Y + ShiftY, Y0 + Margin, Y1 - Margin),
					GridSpot.Z);
				Plants.Add({ Location, Scale, SpotSeed, BlockIndex });
			}
		}
	}

	// Lawns: boxes, like every other surface of the city
	{
		UPCGBasePointData* Points = CityPCG::NewPoints(Context, Lawns.Num());
		auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
		auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
		for (int32 Index = 0; Index < Lawns.Num(); ++Index)
		{
			Transforms[Index] = CityPCG::MakeBoxTransform(Lawns[Index].Center, Lawns[Index].Size);
			Seeds[Index] = Lawns[Index].Seed;
		}
		CityPCG::AddOutput(Context, Points, CityPCGNames::LawnsPin);
	}

	// Plants: pivot on the lawn, uniform scale, facing +X (a Transform Points node turns them later)
	{
		UPCGBasePointData* Points = CityPCG::NewPoints(Context, Plants.Num(), FVector(25.0));
		auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
		auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
		auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
		FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
		for (int32 Index = 0; Index < Plants.Num(); ++Index)
		{
			const FPlant& Plant = Plants[Index];
			Transforms[Index] = FTransform(FRotator::ZeroRotator, Plant.Location, FVector(Plant.Scale));
			Seeds[Index] = Plant.Seed;
			BlockIDAttribute->SetValue(Entries[Index], Plant.BlockID);
		}
		CityPCG::AddOutput(Context, Points, CityPCGNames::PlantsPin);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
