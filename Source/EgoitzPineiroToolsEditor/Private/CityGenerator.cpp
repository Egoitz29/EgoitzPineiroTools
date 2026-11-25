#include "CityGenerator.h"

#include "CityGrid.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "ProceduralCityActor.h"

#define LOCTEXT_NAMESPACE "CityGenerator"

UWorld* FCityGenerator::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

TArray<AProceduralCityActor*> FCityGenerator::FindCities(UWorld* World)
{
	TArray<AProceduralCityActor*> Cities;
	if (World)
	{
		for (TActorIterator<AProceduralCityActor> It(World); It; ++It)
		{
			Cities.Add(*It);
		}
	}
	return Cities;
}

bool FCityGenerator::HasCity() const
{
	return FindCities(GetEditorWorld()).Num() > 0;
}

bool FCityGenerator::Generate(bool bFullRebuild, FText& OutMessage)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		OutMessage = LOCTEXT("NoWorld", "No hay ningún nivel abierto en el editor.");
		return false;
	}

	Params.Clamp();

	// A full rebuild starts from nothing: no leftovers of the previous city can survive
	TArray<AProceduralCityActor*> Cities = FindCities(World);
	const bool bNewCity = bFullRebuild || Cities.Num() != 1;
	if (bNewCity)
	{
		Cleanup(); // first, so nothing regenerates while the graphs are being rebuilt
		LastContentReport = FCityContent::Ensure(/*bRebuildGraphs=*/false);
	}

	UPCGGraphInterface* Graph = LoadObject<UPCGGraphInterface>(nullptr, *FCityContent::CityGraphObjectPath());
	if (!Graph)
	{
		OutMessage = FText::Format(LOCTEXT("NoGraph", "No se ha podido crear el grafo {0}. Mira los errores de contenido."), FText::FromString(FCityContent::CityGraphObjectPath()));
		return false;
	}

	AProceduralCityActor* City = nullptr;
	if (bNewCity)
	{
		City = World->SpawnActor<AProceduralCityActor>(FVector::ZeroVector, FRotator::ZeroRotator);
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

	City->SetCityExtent(FCityGrid::Make(Params.CitySize, Params.BlockSize).Extent());

	UPCGComponent* PCG = City->GetPCGComponent();
	PCG->SetGraph(Graph);
	UPCGGraphInstance* Instance = PCG->GetGraphInstance();
	if (!Instance)
	{
		OutMessage = LOCTEXT("NoInstance", "El PCG Component no tiene Graph Instance.");
		return false;
	}

	// The panel values replace the defaults of PCG_City (Graph Parameters of type Double)
	TArray<FString> MissingParams;
	auto SetParameter = [Instance, &MissingParams](const FName Name, double Value)
	{
		if (Instance->SetGraphParameter<double>(Name, Value) != EPropertyBagResult::Success)
		{
			MissingParams.Add(Name.ToString());
		}
	};
	SetParameter(CityParamNames::CitySize, Params.CitySize);
	SetParameter(CityParamNames::BlockSize, Params.BlockSize);
	SetParameter(CityParamNames::BuildingDensity, Params.BuildingDensity);
	SetParameter(CityParamNames::BuildingHeight, Params.BuildingHeight);
	SetParameter(CityParamNames::BuildingVariation, Params.BuildingVariation);
	SetParameter(CityParamNames::ParkRatio, Params.ParkRatio);
	SetParameter(CityParamNames::ParkTreeDensity, Params.ParkTreeDensity);
	SetParameter(CityParamNames::ParkTreeScaleVariation, Params.ParkTreeScaleVariation);

	// Zoning off = every factor 1: the same height and density everywhere
	SetParameter(CityParamNames::ZoneRadius, Params.ZoneRadius);
	SetParameter(CityParamNames::ZoneTransition, Params.ZoneTransition);
	SetParameter(CityParamNames::ZoneCenterHeight, Params.bZoning ? Params.ZoneCenterHeight : 1.0);
	SetParameter(CityParamNames::ZonePeripheryHeight, Params.bZoning ? Params.ZonePeripheryHeight : 1.0);
	SetParameter(CityParamNames::ZoneCenterDensity, Params.bZoning ? Params.ZoneCenterDensity : 1.0);
	SetParameter(CityParamNames::ZonePeripheryDensity, Params.bZoning ? Params.ZonePeripheryDensity : 1.0);

	PCG->Seed = Params.Seed;
	PCG->GenerateLocal(/*bForce=*/true);
	City->MarkPackageDirty();

	if (!MissingParams.IsEmpty())
	{
		OutMessage = FText::Format(LOCTEXT("MissingParams", "Generada, pero PCG_City no tiene estos Graph Parameters de tipo Double: {0}. Pulsa Rehacer grafos."),
			FText::FromString(FString::Join(MissingParams, TEXT(", "))));
		return false;
	}
	if (!LastContentReport.Errors.IsEmpty())
	{
		OutMessage = FText::Format(LOCTEXT("GeneratedWithErrors", "Ciudad generada con seed {0}, pero el contenido tiene errores (abajo)."), Params.Seed);
		return false;
	}

	OutMessage = FText::Format(LOCTEXT("Generated", "Ciudad generada con seed {0}."), Params.Seed);
	return true;
}

int32 FCityGenerator::Cleanup()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return 0;
	}

	// Everything the tool creates carries the tag
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(AProceduralCityActor::GeneratedTag))
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

FCityStats FCityGenerator::ComputeStats() const
{
	FCityStats Stats;
	TMap<FString, int32> PerType;
	uint64 HashSum = 0;

	for (AProceduralCityActor* City : FindCities(GetEditorWorld()))
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
