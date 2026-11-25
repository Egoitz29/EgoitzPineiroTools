#include "CityPCG/PCGCitySidewalkPoints.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCitySidewalkPoints"

namespace
{
	struct FSidewalkPoint
	{
		FVector Location;
		double Yaw;
		int32 BlockID;
		int32 Side;
	};
}

#if WITH_EDITOR
FText UPCGCitySidewalkPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Points along the sidewalks, facing the street and away from the crossings. Spacing and Phase set the pattern.");
}
#endif

TArray<FPCGPinProperties> UPCGCitySidewalkPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCitySidewalkPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::PointsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCitySidewalkPointsSettings::CreateElement() const
{
	return MakeShared<FPCGCitySidewalkPointsElement>();
}

bool FPCGCitySidewalkPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCitySidewalkPointsElement::Execute);

	const UPCGCitySidewalkPointsSettings* Settings = Context->GetInputSettings<UPCGCitySidewalkPointsSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Blocks = CityPCG::ReadBoxes(Context, CityPCGNames::BlocksPin);

	const double Spacing = FMath::Max(Settings->Spacing, 1.0) * 100.0;
	const double Phase = FMath::Clamp(Settings->Phase, 0.0, 1.0);
	const double CurbOffset = FMath::Max(Settings->CurbOffset, 0.0) * 100.0;
	const double Clearance = FMath::Max(Settings->CornerClearance, 0.0) * 100.0;

	TArray<FSidewalkPoint> Found;
	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const CityPCG::FCityBox& Block = Blocks[BlockIndex];

		// The four sides, counter-clockwise, each on the line CurbOffset in from its kerb.
		// Yaw: the direction the side faces (towards its street).
		const struct { FVector2D Start; FVector2D Direction; double Length; double Yaw; } Sides[4] =
		{
			{ FVector2D(Block.MinX(), Block.MinY() + CurbOffset), FVector2D(1.0, 0.0), Block.Size.X, -90.0 }, // 0 south
			{ FVector2D(Block.MaxX() - CurbOffset, Block.MinY()), FVector2D(0.0, 1.0), Block.Size.Y, 0.0 },   // 1 east
			{ FVector2D(Block.MaxX(), Block.MaxY() - CurbOffset), FVector2D(-1.0, 0.0), Block.Size.X, 90.0 }, // 2 north
			{ FVector2D(Block.MinX() + CurbOffset, Block.MaxY()), FVector2D(0.0, -1.0), Block.Size.Y, 180.0 } // 3 west
		};

		for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
		{
			const double Length = Sides[SideIndex].Length;
			const double Usable = Length - 2.0 * Clearance;
			if (Usable < 0.0)
			{
				continue; // the side is too short to keep the corners clear
			}

			// The pattern is centred on the side, then shifted by Phase
			const int32 Count = FMath::FloorToInt32(Usable / Spacing) + 1;
			const double First = Clearance + 0.5 * (Usable - (Count - 1) * Spacing) + Phase * Spacing;

			for (int32 Step = 0; Step < Count; ++Step)
			{
				const double Along = First + Step * Spacing;
				if (Along > Length - Clearance + 1.0)
				{
					break;
				}

				const FVector2D Position = Sides[SideIndex].Start + Sides[SideIndex].Direction * Along;
				Found.Add({ FVector(Position.X, Position.Y, Block.Top()), Sides[SideIndex].Yaw, BlockIndex, SideIndex });
			}
		}
	}

	// Small 50 cm debug box; scale 1 so the meshes keep their own size, pivot on the sidewalk
	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Found.Num(), FVector(25.0));
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);
	FPCGMetadataAttribute<int32>* SideAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Side, 0);

	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		const FSidewalkPoint& Point = Found[Index];
		Transforms[Index] = FTransform(FRotator(0.0, Point.Yaw, 0.0), Point.Location);
		Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Point.Location);
		BlockIDAttribute->SetValue(Entries[Index], Point.BlockID);
		SideAttribute->SetValue(Entries[Index], Point.Side);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::PointsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
