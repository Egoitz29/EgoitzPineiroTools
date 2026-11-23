#include "CityPCG/PCGCitySidewalkSlots.h"

#include "CityPCG/CityPCGNames.h"
#include "CityPCGUtils.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PCGCitySidewalkSlots"

#if WITH_EDITOR
FText UPCGCitySidewalkSlotsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Points along the middle of every sidewalk, facing the street and away from the crossings. PropType says which furniture goes in each one.");
}
#endif

TArray<FPCGPinProperties> UPCGCitySidewalkSlotsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::BlocksPin, EPCGDataType::Point);
	Pins.Emplace(CityPCGNames::CrossingsPin, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCitySidewalkSlotsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(CityPCGNames::SlotsPin, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCitySidewalkSlotsSettings::CreateElement() const
{
	return MakeShared<FPCGCitySidewalkSlotsElement>();
}

namespace
{
	/** Distance in the XY plane from a point to an axis-aligned box. */
	double DistanceToBox2D(const FVector& Point, const CityPCG::FCityBox& Box)
	{
		const double DX = FMath::Max(0.0, FMath::Abs(Point.X - Box.Center.X) - 0.5 * Box.Size.X);
		const double DY = FMath::Max(0.0, FMath::Abs(Point.Y - Box.Center.Y) - 0.5 * Box.Size.Y);
		return FMath::Sqrt(DX * DX + DY * DY);
	}
}

bool FPCGCitySidewalkSlotsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCitySidewalkSlotsElement::Execute);

	const UPCGCitySidewalkSlotsSettings* Settings = Context->GetInputSettings<UPCGCitySidewalkSlotsSettings>();
	check(Settings);

	const TArray<CityPCG::FCityBox> Blocks = CityPCG::ReadBoxes(Context, CityPCGNames::BlocksPin);
	const TArray<CityPCG::FCityBox> Crossings = CityPCG::ReadBoxes(Context, CityPCGNames::CrossingsPin);
	if (Blocks.IsEmpty())
	{
		return true;
	}
	if (Crossings.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoCrossings", "No Crossings connected: slots are not checked against the crossings."));
	}

	const double SidewalkWidth = FMath::Max(Settings->SidewalkWidth, 0.5) * 100.0;
	const double Spacing = FMath::Max(Settings->PropSpacing, 1.0) * 100.0;
	const double Clearance = FMath::Max(Settings->CornerClearance, 0.0) * 100.0;
	const int32 NumPropTypes = FMath::Max(Settings->NumPropTypes, 1);

	struct FSlot
	{
		FVector Location;
		double Yaw;
		int32 PropType;
		int32 Side;
		int32 BlockID;
	};
	TArray<FSlot> Slots;

	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const CityPCG::FCityBox& Block = Blocks[BlockIndex];

		// The middle line of the sidewalk ring: half a sidewalk in from the block edge
		const double Inset = 0.5 * SidewalkWidth;
		const double MinX = Block.Center.X - 0.5 * Block.Size.X + Inset;
		const double MaxX = Block.Center.X + 0.5 * Block.Size.X - Inset;
		const double MinY = Block.Center.Y - 0.5 * Block.Size.Y + Inset;
		const double MaxY = Block.Center.Y + 0.5 * Block.Size.Y - Inset;
		if (MaxX <= MinX || MaxY <= MinY)
		{
			continue;
		}

		// Each side runs counter-clockwise and faces away from the block (towards its street)
		const struct { FVector2D Start; FVector2D End; double Yaw; } Sides[4] =
		{
			{ FVector2D(MinX, MinY), FVector2D(MaxX, MinY), -90.0 }, // 0 South
			{ FVector2D(MaxX, MinY), FVector2D(MaxX, MaxY), 0.0 },   // 1 East
			{ FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY), 90.0 },  // 2 North
			{ FVector2D(MinX, MaxY), FVector2D(MinX, MinY), 180.0 }, // 3 West
		};

		for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
		{
			const FVector2D Start = Sides[SideIndex].Start;
			const FVector2D Direction = Sides[SideIndex].End - Start;
			const double Length = Direction.Size();
			if (Length <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			// Evenly spread and centred: never exactly on a corner, so two sides never share a slot
			const int32 Count = FMath::Max(1, FMath::FloorToInt32(Length / Spacing));
			const double FirstOffset = 0.5 * (Length - (Count - 1) * Spacing);

			for (int32 Step = 0; Step < Count; ++Step)
			{
				const FVector2D Position = Start + Direction / Length * (FirstOffset + Step * Spacing);
				const FVector Location(Position.X, Position.Y, Block.Top());

				bool bTooClose = false;
				for (const CityPCG::FCityBox& Crossing : Crossings)
				{
					if (DistanceToBox2D(Location, Crossing) < Clearance)
					{
						bTooClose = true;
						break;
					}
				}
				if (bTooClose)
				{
					continue;
				}

				// The type depends on the position along the side, not on what was dropped: stable pattern
				Slots.Add({ Location, Sides[SideIndex].Yaw, Step % NumPropTypes, SideIndex, BlockIndex });
			}
		}
	}

	// Slots carry no size: scale 1 so the furniture meshes keep their own size. Small 50 cm debug box.
	UPCGBasePointData* Points = CityPCG::NewPoints(Context, Slots.Num(), FVector(25.0));
	auto Transforms = Points->GetTransformValueRange(/*bAllocate=*/false);
	auto Seeds = Points->GetSeedValueRange(/*bAllocate=*/false);
	auto Entries = Points->GetMetadataEntryValueRange(/*bAllocate=*/false);
	FPCGMetadataAttribute<int32>* PropTypeAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::PropType, 0);
	FPCGMetadataAttribute<int32>* SideAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::Side, 0);
	FPCGMetadataAttribute<int32>* BlockIDAttribute = CityPCG::CreateAttribute<int32>(Points, CityPCGNames::BlockID, 0);

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FSlot& Slot = Slots[Index];
		Transforms[Index] = FTransform(FRotator(0.0, Slot.Yaw, 0.0), Slot.Location); // pivot of the mesh on the sidewalk
		Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Slot.Location);
		PropTypeAttribute->SetValue(Entries[Index], Slot.PropType);
		SideAttribute->SetValue(Entries[Index], Slot.Side);
		BlockIDAttribute->SetValue(Entries[Index], Slot.BlockID);
	}

	CityPCG::AddOutput(Context, Points, CityPCGNames::SlotsPin);
	return true;
}

#undef LOCTEXT_NAMESPACE
