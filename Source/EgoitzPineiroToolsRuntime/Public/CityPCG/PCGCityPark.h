#pragma once

#include "PCGSettings.h"
#include "PCGCityPark.generated.h"

/**
 * Turns each park block into a lawn with trees and bushes.
 *
 * Lawns:  one box per block, the area inside the sidewalk ring, LawnHeight above the sidewalk.
 * Plants: candidate spots on a grid every PlantSpacing metres over the lawn, each moved a little
 *         from its grid position. A spot keeps its plant with probability TreeDensity, and the
 *         plant gets a uniform scale 1 + ScaleVariation * Rand(-0.5, 0.5). Everything comes from
 *         the seed and the spot position. Which mesh grows there (tree or bush) is chosen later by
 *         the Static Mesh Spawner, also from the seed.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityParkSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityPark"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityParkTitle", "City Park"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Share of the candidate spots that get a plant (0 - 1). Connect the ParkTreeDensity Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double TreeDensity = 0.6;

	/** 0 = all plants the same size, 1 = from half to one and a half times. Connect the ParkTreeScaleVariation Graph Parameter here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double ScaleVariation = 0.4;

	/** Width of the sidewalk ring of each block, in metres. The lawn starts inside it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double SidewalkWidth = 3.0;

	/** Distance between two candidate spots, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double PlantSpacing = 6.0;

	/** No plant closer than this to the edge of the lawn, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double EdgeMargin = 1.5;

	/** Height of the lawn above the sidewalk, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.01", ForceUnits = "m"))
	double LawnHeight = 0.1;
};

class FPCGCityParkElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
