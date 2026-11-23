#include "CityGeneratorTool.h"

#include "CityGeneratorActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

#define LOCTEXT_NAMESPACE "CityGeneratorTool"

const TCHAR* FCityGeneratorTool::GraphPackage = TEXT("/EgoitzPineiroTools/City/PCG_City");
const TCHAR* FCityGeneratorTool::GraphObjectPath = TEXT("/EgoitzPineiroTools/City/PCG_City.PCG_City");

const FName FCityGeneratorTool::CitySizeParam(TEXT("CitySize"));
const FName FCityGeneratorTool::BlockSizeParam(TEXT("BlockSize"));
const FName FCityGeneratorTool::BuildingDensityParam(TEXT("BuildingDensity"));
const FName FCityGeneratorTool::BuildingHeightParam(TEXT("BuildingHeight"));
const FName FCityGeneratorTool::BuildingVariationParam(TEXT("BuildingVariation"));

UWorld* FCityGeneratorTool::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

TArray<ACityGeneratorActor*> FCityGeneratorTool::FindCities(UWorld* World)
{
	TArray<ACityGeneratorActor*> Cities;
	if (World)
	{
		for (TActorIterator<ACityGeneratorActor> It(World); It; ++It)
		{
			Cities.Add(*It);
		}
	}
	return Cities;
}

bool FCityGeneratorTool::IsGraphAvailable()
{
	return FPackageName::DoesPackageExist(GraphPackage);
}

bool FCityGeneratorTool::HasCity() const
{
	return FindCities(GetEditorWorld()).Num() > 0;
}

void FCityGeneratorTool::ClampParams()
{
	Params.CitySize = FMath::Clamp(Params.CitySize, 50.0, 3000.0);
	Params.BlockSize = FMath::Clamp(Params.BlockSize, 15.0, 300.0);
	Params.BuildingDensity = FMath::Clamp(Params.BuildingDensity, 0.0, 1.0);
	Params.BuildingHeight = FMath::Clamp(Params.BuildingHeight, 3.0, 300.0);
	Params.BuildingVariation = FMath::Clamp(Params.BuildingVariation, 0.0, 1.0);
	Params.Seed = FMath::Max(Params.Seed, 0);
}

bool FCityGeneratorTool::Generate(bool bFullRebuild, FText& OutMessage)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		OutMessage = LOCTEXT("NoWorld", "No hay ningún nivel abierto en el editor.");
		return false;
	}

	UPCGGraphInterface* Graph = IsGraphAvailable() ? LoadObject<UPCGGraphInterface>(nullptr, GraphObjectPath) : nullptr;
	if (!Graph)
	{
		OutMessage = FText::Format(LOCTEXT("NoGraph", "No se encuentra el grafo {0}. Créalo siguiendo Docs/Grafos.md."), FText::FromString(GraphPackage));
		return false;
	}

	ClampParams();

	// A full rebuild starts from nothing: no leftovers of the previous city can survive
	TArray<ACityGeneratorActor*> Cities = FindCities(World);
	ACityGeneratorActor* City = nullptr;
	if (bFullRebuild || Cities.Num() != 1)
	{
		Cleanup();
		City = World->SpawnActor<ACityGeneratorActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!City)
		{
			OutMessage = LOCTEXT("SpawnFailed", "No se ha podido crear el actor de la ciudad.");
			return false;
		}
		City->SetActorLabel(TEXT("ProceduralCity"));
	}
	else
	{
		City = Cities[0];
	}

	City->SetCitySize(Params.CitySize);

	UPCGComponent* PCG = City->GetPCGComponent();
	PCG->SetGraph(Graph);
	UPCGGraphInstance* Instance = PCG->GetGraphInstance();
	if (!Instance)
	{
		OutMessage = LOCTEXT("NoInstance", "El PCG Component no tiene Graph Instance.");
		return false;
	}

	// The panel values replace the defaults of the graph. Same names and types as in PCG_City.
	TArray<FString> MissingParams;
	auto SetParameter = [Instance, &MissingParams](const FName Name, double Value)
	{
		if (Instance->SetGraphParameter<double>(Name, Value) != EPropertyBagResult::Success)
		{
			MissingParams.Add(Name.ToString());
		}
	};
	SetParameter(CitySizeParam, Params.CitySize);
	SetParameter(BlockSizeParam, Params.BlockSize);
	SetParameter(BuildingDensityParam, Params.BuildingDensity);
	SetParameter(BuildingHeightParam, Params.BuildingHeight);
	SetParameter(BuildingVariationParam, Params.BuildingVariation);

	PCG->Seed = Params.Seed;
	PCG->GenerateLocal(/*bForce=*/true);
	City->MarkPackageDirty();

	if (!MissingParams.IsEmpty())
	{
		OutMessage = FText::Format(LOCTEXT("MissingParams", "Generada, pero PCG_City no tiene estos Graph Parameters de tipo Double: {0}. Esos valores no se aplican."),
			FText::FromString(FString::Join(MissingParams, TEXT(", "))));
		return false;
	}

	OutMessage = FText::Format(LOCTEXT("Generated", "Ciudad generada con seed {0}."), Params.Seed);
	return true;
}

int32 FCityGeneratorTool::Cleanup()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return 0;
	}

	// Everything the tool creates carries the tag: the city actor and anything spawned outside PCG
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(ACityGeneratorActor::GeneratedTag))
		{
			ToDestroy.Add(*It);
		}
	}

	for (AActor* Actor : ToDestroy)
	{
		// First what PCG generated (instances, partition actors...), then the actor itself
		if (UPCGComponent* PCG = Actor->FindComponentByClass<UPCGComponent>())
		{
			PCG->CleanupLocal(/*bRemoveComponents=*/true);
		}
		World->EditorDestroyActor(Actor, /*bShouldModifyLevel=*/true);
	}

	return ToDestroy.Num();
}

FCityStats FCityGeneratorTool::ComputeStats() const
{
	FCityStats Stats;
	TMap<FString, int32> PerType;
	uint64 HashSum = 0;

	for (ACityGeneratorActor* City : FindCities(GetEditorWorld()))
	{
		++Stats.NumCities;
		if (const UPCGComponent* PCG = City->GetPCGComponent())
		{
			Stats.bGenerating |= PCG->IsGenerating();
		}

		// PCG puts every Static Mesh Spawner result in instanced components of the city actor
		TArray<UInstancedStaticMeshComponent*> Components;
		City->GetComponents(Components);
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			const UMaterialInterface* Material = Component->GetMaterial(0);
			const FString Type = FString::Printf(TEXT("%s / %s"), Mesh ? *Mesh->GetName() : TEXT("-"), Material ? *Material->GetName() : TEXT("-"));
			const uint32 TypeHash = GetTypeHash(Type);

			const int32 NumInstances = Component->GetInstanceCount();
			PerType.FindOrAdd(Type) += NumInstances;
			Stats.TotalInstances += NumInstances;

			for (int32 Index = 0; Index < NumInstances; ++Index)
			{
				FTransform Transform;
				Component->GetInstanceTransform(Index, Transform, /*bWorldSpace=*/true);

				// Rounded to cm, tenths of a degree and thousandths of scale: tiny float noise does not count
				const FVector Location = Transform.GetLocation();
				const FRotator Rotation = Transform.Rotator();
				const FVector Scale = Transform.GetScale3D();
				uint32 Hash = HashCombine(TypeHash, GetTypeHash(FIntVector(FMath::RoundToInt32(Location.X), FMath::RoundToInt32(Location.Y), FMath::RoundToInt32(Location.Z))));
				Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(Rotation.Pitch * 10.0), FMath::RoundToInt32(Rotation.Yaw * 10.0), FMath::RoundToInt32(Rotation.Roll * 10.0))));
				Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(Scale.X * 1000.0), FMath::RoundToInt32(Scale.Y * 1000.0), FMath::RoundToInt32(Scale.Z * 1000.0))));

				// A sum does not depend on the order of components or instances
				HashSum += Hash;
			}
		}
	}

	for (const TPair<FString, int32>& Pair : PerType)
	{
		Stats.InstancesPerType.Add(Pair);
	}
	Stats.InstancesPerType.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Key < B.Key; });
	Stats.Fingerprint = static_cast<uint32>(HashSum ^ (HashSum >> 32));
	return Stats;
}

#undef LOCTEXT_NAMESPACE
