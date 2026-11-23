#pragma once

#include "PCGSettings.h"
#include "PCGCityLots.generated.h"

/**
 * Layer 4 of the city: the lots (building plots) inside each block.
 *
 * The block minus its sidewalk ring (SidewalkWidth on every side) is split into a regular grid
 * of lots of about LotSize. Lots on the edge of that grid touch the sidewalk (Frontage = true);
 * the others are courtyards. Every lot faces the side of the block it is closest to (yaw snapped
 * to 90 degrees), so the building that stands on it will face the street.
 * Output attributes: BlockID, Frontage.
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
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Width of the sidewalk ring around each block, in metres. Must match City Sidewalk Slots. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.5", ForceUnits = "m"))
	double SidewalkWidth = 3.0;

	/** Approximate side of a lot, in metres. The real size is adjusted so the lots fill the block exactly. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "2.0", ForceUnits = "m"))
	double LotSize = 20.0;
};

class FPCGCityLotsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
