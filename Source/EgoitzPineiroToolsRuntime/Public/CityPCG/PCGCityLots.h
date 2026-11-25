#pragma once

#include "PCGSettings.h"
#include "PCGCityLots.generated.h"

/**
 * Splits every block into building lots that face the street.
 *
 * Inside the sidewalk ring of each block there is a band LotDepth deep along the four sides; the
 * band is cut into lots about LotWidth wide. The corners belong to the south and north rows, so no
 * two lots overlap. Attributes BlockID and Side.
 *
 * Each lot also gets a $Density that decides whether it is built, so a standard Density Filter
 * (0.5 - 1) does the selection: a seeded random value R in [0, 1) per lot gives
 *
 *   $Density = 1 - 0.5 * R / BuildingDensity
 *
 * which is >= 0.5 exactly when R <= BuildingDensity. With BuildingDensity 0.6, 60 % of the lots
 * pass the filter; with 0, none. Same seed, same lots.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityLotsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityLots"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityLotsTitle", "City Lots"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Share of the lots that get a building (0 - 1). Connect the BuildingDensity Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double BuildingDensity = 0.6;

	/** Width of the sidewalk ring of each block, in metres. Lots start inside it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double SidewalkWidth = 3.0;

	/** Depth of the lots from the sidewalk inwards, in metres (less in small blocks). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "2.0", ForceUnits = "m"))
	double LotDepth = 18.0;

	/** Approximate width of a lot along the street, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "2.0", ForceUnits = "m"))
	double LotWidth = 15.0;
};

class FPCGCityLotsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
