#include "CityPCGUtils.h"

#include "Algo/BinarySearch.h"
#include "PCGContext.h"

namespace CityPCG
{
	TArray<FCityBox> ReadBoxes(const FPCGContext* Context, FName PinLabel)
	{
		TArray<FCityBox> Boxes;
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PinLabel);

		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGBasePointData* Points = Cast<UPCGBasePointData>(Input.Data);
			if (!Points)
			{
				continue;
			}

			const int32 NumPoints = Points->GetNumPoints();
			const auto Transforms = Points->GetConstTransformValueRange();
			const auto BoundsMin = Points->GetConstBoundsMinValueRange();
			const auto BoundsMax = Points->GetConstBoundsMaxValueRange();
			const auto Seeds = Points->GetConstSeedValueRange();

			Boxes.Reserve(Boxes.Num() + NumPoints);
			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				const FTransform Transform = Transforms[Index];
				const FVector Min = BoundsMin[Index];
				const FVector Max = BoundsMax[Index];

				FCityBox& Box = Boxes.AddDefaulted_GetRef();
				Box.Center = Transform.TransformPosition(0.5 * (Min + Max));
				Box.Size = (Max - Min) * Transform.GetScale3D().GetAbs(); // real size = bounds x scale
				Box.Yaw = Transform.Rotator().Yaw;
				Box.Seed = Seeds[Index];
			}
		}

		return Boxes;
	}

	FTransform MakeBoxTransform(const FVector& Center, const FVector& Size, double YawDegrees)
	{
		return FTransform(FRotator(0.0, YawDegrees, 0.0), Center, Size / 100.0);
	}

	UPCGBasePointData* NewPoints(FPCGContext* Context, int32 NumPoints, const FVector& Extents)
	{
		UPCGBasePointData* Points = FPCGContext::NewPointData_AnyThread(Context);
		Points->SetNumPoints(NumPoints);
		Points->SetExtents(Extents);
		Points->AllocateProperties(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed | EPCGPointNativeProperties::MetadataEntry);

		// One metadata entry per point, so each point can hold its own attribute values
		UPCGMetadata* Metadata = Points->MutableMetadata();
		auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			Entries[Index] = Metadata->AddEntry();
		}

		return Points;
	}

	void AddOutput(FPCGContext* Context, UPCGData* Data, FName PinLabel)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = Data;
		Output.Pin = PinLabel;
	}

	namespace
	{
		/** Sorted distinct values: two values closer than Tolerance count as the same street line. */
		TArray<double> DistinctSorted(TArray<double> Values, double Tolerance)
		{
			Values.Sort();
			TArray<double> Result;
			for (const double Value : Values)
			{
				if (Result.IsEmpty() || Value - Result.Last() > Tolerance)
				{
					Result.Add(Value);
				}
			}
			return Result;
		}

		int32 NearestIndex(const TArray<double>& Sorted, double Value)
		{
			const int32 Index = Algo::LowerBound(Sorted, Value);
			if (Index >= Sorted.Num())
			{
				return Sorted.Num() - 1;
			}
			if (Index > 0 && FMath::Abs(Sorted[Index - 1] - Value) < FMath::Abs(Sorted[Index] - Value))
			{
				return Index - 1;
			}
			return Index;
		}
	}

	FCrossingGrid BuildCrossingGrid(const TArray<FCityBox>& Crossings)
	{
		constexpr double Tolerance = 10.0; // cm

		FCrossingGrid Grid;
		TArray<double> Xs;
		TArray<double> Ys;
		for (const FCityBox& Crossing : Crossings)
		{
			Xs.Add(Crossing.Center.X);
			Ys.Add(Crossing.Center.Y);
		}
		Grid.Xs = DistinctSorted(MoveTemp(Xs), Tolerance);
		Grid.Ys = DistinctSorted(MoveTemp(Ys), Tolerance);

		Grid.Cells.Reserve(Crossings.Num());
		for (int32 Index = 0; Index < Crossings.Num(); ++Index)
		{
			const FIntPoint Cell(NearestIndex(Grid.Xs, Crossings[Index].Center.X), NearestIndex(Grid.Ys, Crossings[Index].Center.Y));
			Grid.Cells.Add(Cell);
			Grid.CellToIndex.Add(Cell, Index);
		}

		return Grid;
	}
}
