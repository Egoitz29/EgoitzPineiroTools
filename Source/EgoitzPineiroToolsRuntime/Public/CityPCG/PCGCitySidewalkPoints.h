#pragma once

#include "PCGSettings.h"
#include "PCGCitySidewalkPoints.generated.h"

/**
 * Points along the sidewalks of every block, for street furniture.
 *
 * On each side of a block the points run along a line CurbOffset in from the kerb, every Spacing
 * metres, and never closer than CornerClearance to a corner (so never in a crossing). Phase moves
 * the whole pattern a fraction of Spacing: two kinds of furniture with the same Spacing and
 * Phases 0 and 0.5 alternate without ever sharing a spot. A new kind of furniture is a new node
 * with its own Phase; the existing ones do not change.
 *
 * Each point faces its street (yaw) and has scale 1, so the meshes keep their own size.
 * Attributes BlockID and Side.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class EGOITZPINEIROTOOLSRUNTIME_API UPCGCitySidewalkPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CitySidewalkPoints"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("EgoitzPineiroTools", "CitySidewalkPointsTitle", "City Sidewalk Points"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Distance between two points of the same kind along the sidewalk, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "1.0", ForceUnits = "m"))
	double Spacing = 24.0;

	/** Shift of the pattern, as a fraction of Spacing (0 - 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	double Phase = 0.0;

	/** Distance from the kerb (the edge of the block) to the points, in metres. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double CurbOffset = 0.6;

	/** No point closer than this to a corner of the block, in metres. Keeps the crossings clear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0", ForceUnits = "m"))
	double CornerClearance = 6.0;
};

class FPCGCitySidewalkPointsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
