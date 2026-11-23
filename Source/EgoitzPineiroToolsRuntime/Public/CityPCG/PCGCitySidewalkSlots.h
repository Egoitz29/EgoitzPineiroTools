#pragma once

#include "PCGSettings.h"
#include "PCGCitySidewalkSlots.generated.h"

/**
 * Layer 6 of the city: the places on the sidewalks where street furniture can stand.
 *
 * Points run along the middle of the sidewalk ring of every block, every PropSpacing metres,
 * facing the street. Points closer than CornerClearance to a crossing are dropped, so the
 * crossings and corners stay free. PropType cycles 0, 1, ... NumPropTypes - 1 along each side:
 * each furniture subgraph keeps the points of its own type with an Attribute Filter.
 * Output attributes: PropType, Side (0 S, 1 E, 2 N, 3 W), BlockID.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCitySidewalkSlotsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CitySidewalkSlots"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CitySidewalkSlotsTitle", "City Sidewalk Slots"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Width of the sidewalk ring, in metres. Must match City Lots. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.5", ForceUnits = "m"))
	double SidewalkWidth = 3.0;

	/** Distance between two consecutive slots along a sidewalk, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double PropSpacing = 12.0;

	/** No slot closer than this to a crossing, in metres: keeps the crossings and corners free. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double CornerClearance = 5.0;

	/** How many kinds of street furniture share the slots. Add one and a new subgraph to add a new kind. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1"))
	int32 NumPropTypes = 2;
};

class FPCGCitySidewalkSlotsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
