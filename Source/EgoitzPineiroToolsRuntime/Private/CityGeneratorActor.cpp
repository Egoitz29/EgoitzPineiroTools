#include "CityGeneratorActor.h"

#include "Components/BoxComponent.h"
#include "PCGComponent.h"

const FName ACityGeneratorActor::GeneratedTag(TEXT("EPT_CityGenerated"));

ACityGeneratorActor::ACityGeneratorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CityBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CityBounds"));
	CityBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityBounds->SetBoxExtent(FVector(20000.0, 20000.0, 5000.0));
	RootComponent = CityBounds;

	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	// Only the tool decides when to generate: nothing regenerates by itself when the level loads.
	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;

	Tags.Add(GeneratedTag);
}

void ACityGeneratorActor::SetCitySize(double CitySizeMeters)
{
	const double HalfSide = FMath::Max(CitySizeMeters, 1.0) * 50.0; // metres -> cm, halved
	CityBounds->SetBoxExtent(FVector(HalfSide, HalfSide, 5000.0));
}
