#include "CityPCG/PCGCityLots.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Math/RandomStream.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityLots"

namespace
{
	constexpr double MinLotSize = 200.0;  // cm: anything narrower is not a lot
	constexpr double LotThickness = 10.0; // cm: lots are flat boxes on top of the block

	struct FLot
	{
		FVector Center;
		FVector Size;
		int32 BlockID;
		int32 Side;
	};
}

#if WITH_EDITOR
FText UPCGCityLotsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Building lots facing the street inside each block. $Density >= 0.5 means the lot is built (BuildingDensity, changed by the zone).");
}
#endif

TArray<FPCGPinProperties> UPCGCityLotsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCityLotsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::LotsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityLotsSettings::CreateElement() const
{
	return MakeShared<FPCGCityLotsElement>();
}

bool FPCGCityLotsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityLotsElement::Execute);

	const UPCGCityLotsSettings* Settings = Context->GetInputSettings<UPCGCityLotsSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Blocks = CityPCG::ReadBoxes(Context, CityPCGNames::BlocksPin);

	const double BuildingDensity = FMath::Clamp(Settings->BuildingDensity, 0.0, 1.0);
	const double Sidewalk = FMath::Max(Settings->SidewalkWidth, 0.0) * 100.0;
	const double TargetDepth = FMath::Max(Settings->LotDepth, 2.0) * 100.0;
	const double TargetWidth = FMath::Max(Settings->LotWidth, 2.0) * 100.0;
	const double ZoneRadius = FMath::Max(Settings->ZoneRadius, 1.0) * 100.0;
	const double CenterFactor = FMath::Max(Settings->CenterDensityFactor, 0.0);
	const double PeripheryFactor = FMath::Max(Settings->PeripheryDensityFactor, 0.0);
	const int32 GraphSeed = Context->GetSeed(); // node seed combined with the PCG Component seed

	TArray<FLot> Lots;
	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const CityPCG::FCityBox& Block = Blocks[BlockIndex];

		// The area inside the sidewalk ring
		const double X0 = Block.MinX() + Sidewalk;
		const double X1 = Block.MaxX() - Sidewalk;
		const double Y0 = Block.MinY() + Sidewalk;
		const double Y1 = Block.MaxY() - Sidewalk;
		if (X1 - X0 < MinLotSize || Y1 - Y0 < MinLotSize)
		{
			continue;
		}

		const double Depth = FMath::Min(TargetDepth, 0.5 * FMath::Min(X1 - X0, Y1 - Y0));
		const double Z = Block.Top() + 0.5 * LotThickness;

		// A row of lots between Start and End along one axis, from Near to Far across it
		auto AddRow = [&](double Start, double End, double Near, double Far, bool bAlongX, int32 Side)
		{
			const double Length = End - Start;
			if (Length < MinLotSize)
			{
				return;
			}
			const int32 Count = FMath::Max(1, FMath::FloorToInt32(Length / TargetWidth + 0.5));
			const double Width = Length / Count;
			const double Across = 0.5 * (Near + Far);
			const double DepthSize = FMath::Abs(Far - Near);

			for (int32 Step = 0; Step < Count; ++Step)
			{
				const double Along = Start + (Step + 0.5) * Width;
				FLot& Lot = Lots.AddDefaulted_GetRef();
				Lot.Center = bAlongX ? FVector(Along, Across, Z) : FVector(Across, Along, Z);
				Lot.Size = bAlongX ? FVector(Width, DepthSize, LotThickness) : FVector(DepthSize, Width, LotThickness);
				Lot.BlockID = BlockIndex;
				Lot.Side = Side;
			}
		};

		AddRow(X0, X1, Y0, Y0 + Depth, /*bAlongX=*/true, 0);                 // south, corners included
		AddRow(Y0 + Depth, Y1 - Depth, X1 - Depth, X1, /*bAlongX=*/false, 1); // east, between the corners
		AddRow(X0, X1, Y1 - Depth, Y1, /*bAlongX=*/true, 2);                 // north, corners included
		AddRow(Y0 + Depth, Y1 - Depth, X0, X0 + Depth, /*bAlongX=*/false, 3); // west, between the corners
	}

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Lots.Num(), FVector(50.0), EPCGPointNativeProperties::Density);
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Densities = Points->GetDensityValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
	FPCGMetadataAttribute<int32>* SideAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Side, 0);
	FPCGMetadataAttribute<double>* ZoneAttribute = CityPCG::CreateAttribute<double>(Points, CityPCGNames::Zone, 0.0);

	for (int32 Index = 0; Index < Lots.Num(); ++Index)
	{
		const FLot& Lot = Lots[Index];
		const int32 LotSeed = PCGHelpers::ComputeSeedFromPosition(Lot.Center);

		// Density of the lot's zone: from the periphery factor to the centre factor along the gradient
		const double Zone = CityPCG::ZoneWeight(Lot.Center, ZoneRadius, Settings->ZoneTransition);
		const double LotDensity = FMath::Clamp(BuildingDensity * FMath::Lerp(PeripheryFactor, CenterFactor, Zone), 0.0, 1.0);

		// One random stream per lot: the lot decides alone, whatever happens to the others
		FRandomStream Random(PCGHelpers::ComputeSeed(GraphSeed, LotSeed));
		const double Roll = Random.FRand();
		const double Density = LotDensity <= 0.0 ? 0.0 : FMath::Clamp(1.0 - 0.5 * Roll / LotDensity, 0.0, 1.0);

		Transforms[Index] = CityPCG::MakeBoxTransform(Lot.Center, Lot.Size);
		Seeds[Index] = LotSeed;
		Densities[Index] = static_cast<float>(Density);
		BlockIDAttribute->SetValue(Entries[Index], Lot.BlockID);
		SideAttribute->SetValue(Entries[Index], Lot.Side);
		ZoneAttribute->SetValue(Entries[Index], Zone);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::LotsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
