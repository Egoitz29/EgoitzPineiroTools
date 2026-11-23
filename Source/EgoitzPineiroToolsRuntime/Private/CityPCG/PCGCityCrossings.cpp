#include "CityPCG/PCGCityCrossings.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityCrossings"

namespace
{
	// 200 x 200 crossings is a 40 000-crossing city: more is almost surely a typo in the parameters
	constexpr int32 MaxCrossingsPerSide = 200;
}

#if WITH_EDITOR
FText UPCGCityCrossingsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Square grid of street crossings centred on the origin, with Col and Row attributes. First layer of the city.");
}
#endif

TArray<FPCGPinProperties> UPCGCityCrossingsSettings::InputPinProperties() const
{
	return {}; // a generator: it only needs its parameters
}

TArray<FPCGPinProperties> UPCGCityCrossingsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::CrossingsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityCrossingsSettings::CreateElement() const
{
	return MakeShared<FPCGCityCrossingsElement>();
}

bool FPCGCityCrossingsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityCrossingsElement::Execute);

	const UPCGCityCrossingsSettings* Settings = Context->GetInputSettings<UPCGCityCrossingsSettings>();
	check(Settings);

	// Unreal works in cm, the parameters come in metres
	const double StreetWidth = FMath::Max(Settings->StreetWidth, 2.0) * 100.0;
	const double Thickness = FMath::Max(Settings->RoadThickness, 0.01) * 100.0;
	const double Cell = FMath::Max(Settings->BlockSize, 5.0) * 100.0 + StreetWidth;

	int32 Count = FMath::FloorToInt32(FMath::Max(Settings->CitySize, 0.0) * 100.0 / Cell) + 1;
	if (Count < 2)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("TooSmall", "CitySize is smaller than one block: generating a single block."));
		Count = 2;
	}
	else if (Count > MaxCrossingsPerSide)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("TooBig", "CitySize / BlockSize gives more than {0} crossings per side: clamped."), MaxCrossingsPerSide));
		Count = MaxCrossingsPerSide;
	}

	const double Origin = -0.5 * (Count - 1) * Cell; // centred on the world origin

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Count * Count);
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);

	FPCGMetadataAttribute<int32>* ColAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Col, 0);
	FPCGMetadataAttribute<int32>* RowAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Row, 0);

	int32 Index = 0;
	for (int32 Row = 0; Row < Count; ++Row)
	{
		for (int32 Col = 0; Col < Count; ++Col, ++Index)
		{
			// The slab stands on Z = 0: its centre is half a thickness up
			const FVector Center(Origin + Col * Cell, Origin + Row * Cell, 0.5 * Thickness);
			Transforms[Index] = CityPCG::MakeBoxTransform(Center, FVector(StreetWidth, StreetWidth, Thickness));
			Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Center);
			ColAttribute->SetValue(Entries[Index], Col);
			RowAttribute->SetValue(Entries[Index], Row);
		}
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::CrossingsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
