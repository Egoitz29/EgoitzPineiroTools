#pragma once

#include "PCGSettings.h"
#include "PCGCityBuildings.generated.h"

/**
 * Layer 5 of the city: one building box on each lot that reaches this node.
 *
 * Which lots get a building is decided before, with standard PCG nodes (Attribute Filter on
 * Frontage, Attribute Noise on $Density, Density Filter with BuildingDensity). This node only
 * decides the shape, and every random value comes from the PCG seed and the lot's own $Seed:
 *
 * height    = Max(MinHeight, BuildingHeight * (1 + BuildingVariation * Rand(-0.75, 0.75)))
 * footprint = lot * (MaxFootprint - FootprintVariation * BuildingVariation * Rand(0, 1)), per axis
 *
 * The footprint is always smaller than the lot, so a building never reaches the sidewalk.
 * The building keeps the lot's yaw (it faces the street). Output attributes: Height (m), Floors.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityBuildingsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityBuildings"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityBuildingsTitle", "City Buildings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Average building height, in metres. Connect the BuildingHeight Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double BuildingHeight = 30.0;

	/** 0 = all buildings alike, 1 = very different heights and footprints. Connect the BuildingVariation Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double BuildingVariation = 0.5;

	/** No building is lower than this, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double MinHeight = 3.0;

	/** Largest share of the lot a building covers (per axis). Below 1 so there is always a gap between buildings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.1", ClampMax = "1.0"))
	double MaxFootprint = 0.9;

	/** How much smaller the footprint can get when BuildingVariation is 1 (share of the lot). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "0.9"))
	double FootprintVariation = 0.3;
};

class FPCGCityBuildingsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
