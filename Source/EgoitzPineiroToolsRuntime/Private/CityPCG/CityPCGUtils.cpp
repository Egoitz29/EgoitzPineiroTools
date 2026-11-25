#include "CityPCGUtils.h"

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

	UPCGBasePointData* NewPoints(FPCGContext* Context, int32 NumPoints, const FVector& Extents, EPCGPointNativeProperties ExtraProperties)
	{
		UPCGBasePointData* Points = FPCGContext::NewPointData_AnyThread(Context);
		Points->SetNumPoints(NumPoints);
		Points->SetExtents(Extents);
		Points->AllocateProperties(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed | EPCGPointNativeProperties::MetadataEntry | ExtraProperties);

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

	double ZoneWeight(const FVector& Location, double RadiusCm, double Transition)
	{
		const double Distance = FVector2D(Location.X, Location.Y).Size();
		const double Radius = FMath::Max(RadiusCm, 1.0);
		const double Width = 2.0 * FMath::Clamp(Transition, 0.0, 1.0) * Radius;
		if (Width < 1.0)
		{
			return Distance <= Radius ? 1.0 : 0.0;
		}
		return 1.0 - FMath::SmoothStep(Radius - 0.5 * Width, Radius + 0.5 * Width, Distance);
	}
}
