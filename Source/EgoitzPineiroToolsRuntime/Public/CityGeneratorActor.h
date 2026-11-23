#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityGeneratorActor.generated.h"

class UBoxComponent;
class UPCGComponent;

/**
 * The actor that holds a generated city: a box with the size of the city and the PCG Component
 * that runs PCG_City. Everything PCG spawns lives inside this actor, so destroying it removes
 * the whole city. The editor panel creates, regenerates and destroys it; nobody has to place it by hand.
 */
UCLASS(BlueprintType)
class EGOITZPINEIROTOOLSRUNTIME_API ACityGeneratorActor : public AActor
{
	GENERATED_BODY()

public:
	ACityGeneratorActor();

	/** Tag carried by every actor the city tool creates. Cleanup removes all actors with this tag. */
	static const FName GeneratedTag;

	UPCGComponent* GetPCGComponent() const { return PCGComponent; }

	/** Resizes the bounds box to a square city of the given side, in metres. */
	void SetCitySize(double CitySizeMeters);

private:
	/** Square area of the city. Only for reference in the viewport: the graph centres the city on the world origin. */
	UPROPERTY(VisibleAnywhere, Category = "City")
	TObjectPtr<UBoxComponent> CityBounds;

	/** Runs PCG_City. The panel sets its graph, its Graph Parameters and its Seed. */
	UPROPERTY(VisibleAnywhere, Category = "City")
	TObjectPtr<UPCGComponent> PCGComponent;
};
