#pragma once

#include "PCGSettings.h"
#include "PCGCityCrossings.generated.h"

/**
 * Layer 1 of the city: a square grid of street crossings, centred on the world origin.
 *
 * cell  = BlockSize + StreetWidth
 * count = Floor(CitySize / cell) + 1 crossings per side
 *
 * Each crossing is a StreetWidth x StreetWidth road box, with the integer attributes Col and Row.
 * Everything else (street segments, blocks, lots, sidewalks) is derived from these points.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityCrossingsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityCrossings"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityCrossingsTitle", "City Crossings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Side of the square city, in metres. Connect the CitySize Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "10.0", ForceUnits = "m"))
	double CitySize = 400.0;

	/** Side of a block (sidewalk included), in metres. Connect the BlockSize Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "5.0", ForceUnits = "m"))
	double BlockSize = 60.0;

	/** Width of the road, in metres. Internal constant: the other layers read it from the crossing boxes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "2.0", ForceUnits = "m"))
	double StreetWidth = 8.0;

	/** Thickness of the road slab, in metres. The road surface ends at this height. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.01", ForceUnits = "m"))
	double RoadThickness = 0.05;
};

class FPCGCityCrossingsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
