#pragma once

#include "PCGSettings.h"
#include "PCGCityStreetSegments.generated.h"

/**
 * Layer 2 of the city: the street segments that join neighbouring crossings.
 *
 * Two crossings are neighbours when they are in the same row and consecutive columns (a street
 * along X) or in the same column and consecutive rows (a street along Y). The segment starts
 * where one crossing box ends and ends where the next one begins, so roads never overlap.
 * Output attribute: Axis (0 = along X, 1 = along Y).
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityStreetSegmentsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityStreetSegments"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityStreetSegmentsTitle", "City Street Segments"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGCityStreetSegmentsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
