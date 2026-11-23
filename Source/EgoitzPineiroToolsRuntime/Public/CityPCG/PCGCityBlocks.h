#pragma once

#include "PCGSettings.h"
#include "PCGCityBlocks.generated.h"

/**
 * Layer 3 of the city: the blocks, i.e. the space left between the streets.
 *
 * A block exists wherever four crossings form a cell: (Col, Row), (Col+1, Row), (Col, Row+1)
 * and (Col+1, Row+1). It fills the gap between their road boxes exactly, so a block can never
 * invade the road. Each block is a raised slab (the sidewalk level) of SidewalkHeight.
 * Output attributes: BlockID, Col, Row.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityBlocksSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityBlocks"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityBlocksTitle", "City Blocks"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Height of the sidewalk (the top of every block) above the ground, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.01", ForceUnits = "m"))
	double SidewalkHeight = 0.15;
};

class FPCGCityBlocksElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
