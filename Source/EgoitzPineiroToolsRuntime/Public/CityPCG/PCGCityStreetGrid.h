#pragma once

#include "PCGSettings.h"
#include "PCGCityStreetGrid.generated.h"

/**
 * First layer of the city: the street grid and the blocks between the streets (FCityGrid).
 *
 * Roads:  one box per crossing (StreetWidth x StreetWidth) and one per street between two
 *         crossings. Attributes RoadKind, Axis, Col, Row.
 * Blocks: one box per gap between four crossings, BlockSize x BlockSize, raised SidewalkHeight
 *         (it is the sidewalk slab). Attributes BlockID, Col, Row.
 *
 * Blocks touch the roads but never overlap them. Every other layer starts from Blocks.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCityStreetGridSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CityStreetGrid"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CityStreetGridTitle", "City Street Grid"); }
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

	/** Width of every street, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double StreetWidth = 8.0;

	/** Thickness of the road slabs, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.01", ForceUnits = "m"))
	double RoadThickness = 0.05;

	/** Height of the blocks above the road (the kerb), in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.01", ForceUnits = "m"))
	double SidewalkHeight = 0.15;
};

class FPCGCityStreetGridElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
