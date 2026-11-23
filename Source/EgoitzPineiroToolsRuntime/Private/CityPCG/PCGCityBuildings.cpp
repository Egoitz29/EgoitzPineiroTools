#include "CityPCG/PCGCityBuildings.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "Math/RandomStream.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCityBuildings"

namespace
{
	constexpr double FloorHeight = 350.0; // cm, only for the Floors attribute
}

#if WITH_EDITOR
FText UPCGCityBuildingsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Turns each lot into a building box: height and footprint vary with BuildingVariation, always from the seed.");
}
#endif

TArray<FPCGPinProperties> UPCGCityBuildingsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::LotsPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCityBuildingsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BuildingsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCityBuildingsSettings::CreateElement() const
{
	return MakeShared<FPCGCityBuildingsElement>();
}

bool FPCGCityBuildingsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCityBuildingsElement::Execute);

	const UPCGCityBuildingsSettings* Settings = Context->GetInputSettings<UPCGCityBuildingsSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Lots = CityPCG::ReadBoxes(Context, CityPCGNames::LotsPin);

	const double AverageHeight = FMath::Max(Settings->BuildingHeight, 1.0) * 100.0;
	const double MinHeight = FMath::Max(Settings->MinHeight, 1.0) * 100.0;
	const double Variation = FMath::Clamp(Settings->BuildingVariation, 0.0, 1.0);
	const double MaxFootprint = FMath::Clamp(Settings->MaxFootprint, 0.1, 1.0);
	const double FootprintVariation = FMath::Clamp(Settings->FootprintVariation, 0.0, MaxFootprint - 0.05);
	const int32 GraphSeed = Context->GetSeed(); // node seed combined with the PCG Component seed

	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Lots.Num());
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<double>* HeightAttribute = CityPCG::CreateAttribute<double>(Points, CityPCGNames::Height, 0.0);
	FPCGMetadataAttribute<int32>* FloorsAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Floors, 0);

	for (int32 Index = 0; Index < Lots.Num(); ++Index)
	{
		const CityPCG::FCityBox& Lot = Lots[Index];

		// One random stream per lot: changing one lot never changes the others
		FRandomStream Random(PCGHelpers::ComputeSeed(GraphSeed, Lot.Seed));

		const double Spread = Variation * Random.FRandRange(-0.75f, 0.75f);
		const double Height = FMath::Max(MinHeight, AverageHeight * (1.0 + Spread));
		const double FootprintX = MaxFootprint - FootprintVariation * Variation * Random.FRand();
		const double FootprintY = MaxFootprint - FootprintVariation * Variation * Random.FRand();

		const FVector Size(Lot.Size.X * FootprintX, Lot.Size.Y * FootprintY, Height);
		const FVector Center(Lot.Center.X, Lot.Center.Y, Lot.Bottom() + 0.5 * Height); // standing on the lot

		Transforms[Index] = CityPCG::MakeBoxTransform(Center, Size, Lot.Yaw);
		Seeds[Index] = Lot.Seed;
		HeightAttribute->SetValue(Entries[Index], Height / 100.0);
		FloorsAttribute->SetValue(Entries[Index], FMath::Max(1, FMath::FloorToInt32(Height / FloorHeight)));
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::BuildingsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
