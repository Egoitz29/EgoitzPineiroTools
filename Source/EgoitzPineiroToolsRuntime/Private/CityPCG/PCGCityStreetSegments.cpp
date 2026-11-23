#include "CityPCG/PCGCityStreetSegments.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityStreetSegments"

#if WITH_EDITOR
FText UPCGCityStreetSegmentsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Road segments between neighbouring crossings. They end where the crossing boxes begin, so nothing overlaps.");
}
#endif

TArray<FPCGPinProperties> UPCGCityStreetSegmentsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::CrossingsPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCityStreetSegmentsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::SegmentsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityStreetSegmentsSettings::CreateElement() const
{
	return MakeShared<FPCGCityStreetSegmentsElement>();
}

bool FPCGCityStreetSegmentsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityStreetSegmentsElement::Execute);

	const TArray<CityPCG::FCityBox> Crossings = CityPCG::ReadBoxes(Context, CityPCGNames::CrossingsPin);
	if (Crossings.Num() < 2)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoCrossings", "Street Segments needs at least two crossings on the Crossings pin."));
		return true;
	}

	const CityPCG::FCrossingGrid Grid = CityPCG::BuildCrossingGrid(Crossings);

	struct FSegment
	{
		FVector Center;
		FVector Size;
		int32 Axis;
	};
	TArray<FSegment> Segments;
	Segments.Reserve(Crossings.Num() * 2);

	for (int32 Index = 0; Index < Crossings.Num(); ++Index)
	{
		const CityPCG::FCityBox& A = Crossings[Index];
		const FIntPoint Cell = Grid.Cells[Index];

		// Street along X, towards the next column
		const int32 East = Grid.Find(Cell.X + 1, Cell.Y);
		if (East != INDEX_NONE)
		{
			const CityPCG::FCityBox& B = Crossings[East];
			const double Start = A.Center.X + 0.5 * A.Size.X;
			const double End = B.Center.X - 0.5 * B.Size.X;
			if (End - Start > 1.0)
			{
				const FVector Center(0.5 * (Start + End), 0.5 * (A.Center.Y + B.Center.Y), A.Center.Z);
				Segments.Add({ Center, FVector(End - Start, FMath::Min(A.Size.Y, B.Size.Y), A.Size.Z), 0 });
			}
		}

		// Street along Y, towards the next row
		const int32 North = Grid.Find(Cell.X, Cell.Y + 1);
		if (North != INDEX_NONE)
		{
			const CityPCG::FCityBox& B = Crossings[North];
			const double Start = A.Center.Y + 0.5 * A.Size.Y;
			const double End = B.Center.Y - 0.5 * B.Size.Y;
			if (End - Start > 1.0)
			{
				const FVector Center(0.5 * (A.Center.X + B.Center.X), 0.5 * (Start + End), A.Center.Z);
				Segments.Add({ Center, FVector(FMath::Min(A.Size.X, B.Size.X), End - Start, A.Size.Z), 1 });
			}
		}
	}

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Segments.Num());
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* AxisAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Axis, 0);

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FSegment& Segment = Segments[Index];
		Transforms[Index] = CityPCG::MakeBoxTransform(Segment.Center, Segment.Size);
		Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Segment.Center);
		AxisAttribute->SetValue(Entries[Index], Segment.Axis);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::SegmentsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
