#include "CityPCG/PCGCityLots.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityLots"

#if WITH_EDITOR
FText UPCGCityLotsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Splits the inside of each block (without its sidewalk) into lots facing the street. Frontage marks the lots that touch the sidewalk.");
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
	if (Blocks.IsEmpty())
	{
		return true; // no blocks, no lots
	}

	const double SidewalkWidth = FMath::Max(Settings->SidewalkWidth, 0.5) * 100.0;
	const double TargetLotSize = FMath::Max(Settings->LotSize, 2.0) * 100.0;
	constexpr double LotThickness = 10.0; // cm: a lot is a flat footprint on top of the block

	struct FLot
	{
		FVector Center;
		FVector LocalSize; // X = towards the street, after the yaw
		double Yaw;
		int32 BlockID;
		bool bFrontage;
	};
	TArray<FLot> Lots;

	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const CityPCG::FCityBox& Block = Blocks[BlockIndex];

		// Buildings only go inside the sidewalk ring
		const double InnerX = Block.Size.X - 2.0 * SidewalkWidth;
		const double InnerY = Block.Size.Y - 2.0 * SidewalkWidth;
		if (InnerX < 100.0 || InnerY < 100.0)
		{
			continue; // the sidewalk takes the whole block
		}

		const int32 NumX = FMath::Max(1, FMath::RoundToInt32(InnerX / TargetLotSize));
		const int32 NumY = FMath::Max(1, FMath::RoundToInt32(InnerY / TargetLotSize));
		const double LotX = InnerX / NumX; // the lots fill the block exactly
		const double LotY = InnerY / NumY;
		const double Ground = Block.Top();

		for (int32 J = 0; J < NumY; ++J)
		{
			for (int32 I = 0; I < NumX; ++I)
			{
				FLot& Lot = Lots.AddDefaulted_GetRef();
				Lot.Center = FVector(
					Block.Center.X - 0.5 * InnerX + (I + 0.5) * LotX,
					Block.Center.Y - 0.5 * InnerY + (J + 0.5) * LotY,
					Ground + 0.5 * LotThickness);
				Lot.BlockID = BlockIndex;
				Lot.bFrontage = I == 0 || J == 0 || I == NumX - 1 || J == NumY - 1;

				// Face the closest side of the block, snapped to 90 degrees
				const double DX = (Lot.Center.X - Block.Center.X) / InnerX;
				const double DY = (Lot.Center.Y - Block.Center.Y) / InnerY;
				if (FMath::Abs(DX) >= FMath::Abs(DY))
				{
					Lot.Yaw = DX >= 0.0 ? 0.0 : 180.0;
					Lot.LocalSize = FVector(LotX, LotY, LotThickness);
				}
				else
				{
					Lot.Yaw = DY >= 0.0 ? 90.0 : -90.0;
					Lot.LocalSize = FVector(LotY, LotX, LotThickness); // rotated 90: local X is world Y
				}
			}
		}
	}

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Lots.Num());
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
	FPCGMetadataAttribute<bool>* FrontageAttribute = CityPCG::CreateAttribute<bool>(Points, CityPCGNames::Frontage, false);

	for (int32 Index = 0; Index < Lots.Num(); ++Index)
	{
		const FLot& Lot = Lots[Index];
		Transforms[Index] = CityPCG::MakeBoxTransform(Lot.Center, Lot.LocalSize, Lot.Yaw);
		Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Lot.Center); // stable per lot: later nodes draw from it
		BlockIDAttribute->SetValue(Entries[Index], Lot.BlockID);
		FrontageAttribute->SetValue(Entries[Index], Lot.bFrontage);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::LotsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
