#pragma once

#include "PCGSettings.h"
#include "PCGCityBuildings.generated.h"

/**
 * Turns each lot that reaches it into a building box. Every random value comes from the PCG seed
 * and the lot's own $Seed, so the same seed always gives the same buildings:
 *
 *   height    = Max(MinHeight, H * (1 + BuildingVariation * Rand(-0.75, 0.75)))
 *   H         = BuildingHeight * Lerp(PeripheryHeightFactor, CenterHeightFactor, Zone)
 *   footprint = lot * (MaxFootprint - FootprintVariation * BuildingVariation * Rand(0, 1)), per axis
 *   position  = lot centre + Rand(-1, 1) * Min(PositionJitter, half the free margin), per axis
 *
 * The footprint is always smaller than the lot and the jitter never takes the building out of it,
 * so a building never reaches the sidewalk. Zone is the urban zoning gradient (1 in the centre,
 * 0 in the periphery, see CityPCG::ZoneWeight); with both factors at 1 there is no zoning.
 * Attributes Height (m), Floors and Zone.
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

	/** 0 = all buildings alike, 1 = very different heights, footprints and positions. Connect the BuildingVariation Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double BuildingVariation = 0.5;

	/** No building is lower than this, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double MinHeight = 3.0;

	/** Largest share of the lot a building covers, per axis. Below 1 so there is always a gap between buildings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.1", ClampMax = "0.95"))
	double MaxFootprint = 0.85;

	/** How much smaller the footprint can get when BuildingVariation is 1 (share of the lot). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "0.8"))
	double FootprintVariation = 0.3;

	/** Largest shift of a building inside its lot, in metres (scaled by BuildingVariation). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double PositionJitter = 1.0;

	/** Radius of the centre zone, in metres. Connect the ZoneRadius Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Zoning, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double ZoneRadius = 150.0;

	/** 0 = abrupt edge between centre and periphery, 1 = gradual from the centre to twice the radius. Connect ZoneTransition here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Zoning, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double ZoneTransition = 0.5;

	/** BuildingHeight is multiplied by this in the centre. Connect ZoneCenterHeight here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Zoning, meta = (PCG_Overridable, ClampMin = "0.05"))
	double CenterHeightFactor = 1.0;

	/** BuildingHeight is multiplied by this in the periphery. Connect ZonePeripheryHeight here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Zoning, meta = (PCG_Overridable, ClampMin = "0.05"))
	double PeripheryHeightFactor = 1.0;
};

class FPCGCityBuildingsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
