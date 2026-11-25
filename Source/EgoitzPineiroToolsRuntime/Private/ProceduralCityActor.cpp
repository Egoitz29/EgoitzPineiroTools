#include "ProceduralCityActor.h"

#include "Components/BoxComponent.h"
#include "PCGComponent.h"

const FName AProceduralCityActor::GeneratedTag(TEXT("EPT_CityGenerated"));

AProceduralCityActor::AProceduralCityActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CityBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CityBounds"));
	CityBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityBounds->SetBoxExtent(FVector(20000.0, 20000.0, 5000.0));
	RootComponent = CityBounds;

	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	// Only the tool decides when to generate: nothing regenerates by itself when the level loads
	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;

	Tags.Add(GeneratedTag);
}

void AProceduralCityActor::SetCityExtent(double ExtentCm)
{
	const double Half = 0.5 * FMath::Max(ExtentCm, 100.0);
	CityBounds->SetBoxExtent(FVector(Half, Half, 5000.0));
}
