#include "CityContent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CityParams.h"
#include "CityPCG/CityPCGNames.h"
#include "CityPCG/PCGCityBuildings.h"
#include "CityPCG/PCGCityLots.h"
#include "CityPCG/PCGCityPark.h"
#include "CityPCG/PCGCitySidewalkPoints.h"
#include "CityPCG/PCGCityStreetGrid.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityContent, Log, All);

const TCHAR* FCityContent::RootPath = TEXT("/EgoitzPineiroTools/CityGenerator");

namespace
{
	FString MaterialsPath() { return FString(FCityContent::RootPath) / TEXT("Materials"); }
	FString MeshesPath() { return FString(FCityContent::RootPath) / TEXT("Meshes"); }
	FString SubgraphPackage(const TCHAR* Name) { return FString(FCityContent::RootPath) / TEXT("Subgraphs") / Name; }
	FString CityGraphPackage() { return FString(FCityContent::RootPath) / TEXT("PCG_City"); }

	const TCHAR* const SubgraphNames[] = { TEXT("SG_Streets"), TEXT("SG_Blocks"), TEXT("SG_Buildings"), TEXT("SG_StreetLamps"), TEXT("SG_Trees"), TEXT("SG_Parks") };

	FString ObjectPathOf(const FString& PackageName)
	{
		return PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
	}

	/** Colour of every material instance: all of them are children of M_CityColor. */
	struct FColorDefinition
	{
		const TCHAR* Name;
		FLinearColor Color;
	};

	const FColorDefinition MaterialColors[] =
	{
		{ TEXT("MI_Road"), FLinearColor(0.03f, 0.03f, 0.035f) },
		{ TEXT("MI_Sidewalk"), FLinearColor(0.45f, 0.45f, 0.42f) },
		{ TEXT("MI_Building_A"), FLinearColor(0.70f, 0.62f, 0.50f) },
		{ TEXT("MI_Building_B"), FLinearColor(0.36f, 0.42f, 0.52f) },
		{ TEXT("MI_Building_C"), FLinearColor(0.55f, 0.30f, 0.24f) },
		{ TEXT("MI_LampPost"), FLinearColor(0.08f, 0.08f, 0.08f) },
		{ TEXT("MI_LampLight"), FLinearColor(1.00f, 0.85f, 0.45f) },
		{ TEXT("MI_TreeTrunk"), FLinearColor(0.25f, 0.15f, 0.08f) },
		{ TEXT("MI_TreeLeaves"), FLinearColor(0.10f, 0.35f, 0.10f) },
		{ TEXT("MI_Grass"), FLinearColor(0.16f, 0.40f, 0.12f) },
		{ TEXT("MI_Bush"), FLinearColor(0.06f, 0.24f, 0.07f) },
	};

	/**
	 * Builds a mesh description from simple solids (cylinders and ellipsoids), one material slot per
	 * solid. Pivot at Z = 0: the street furniture stands on the sidewalk points.
	 */
	class FSimpleMeshBuilder
	{
	public:
		FMeshDescription MeshDescription;
		FStaticMeshAttributes Attributes;

		FSimpleMeshBuilder()
			: Attributes(MeshDescription)
		{
			Attributes.Register();
		}

		FPolygonGroupID AddSlot(FName SlotName)
		{
			const FPolygonGroupID Group = MeshDescription.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;
			return Group;
		}

		/** Upright cylinder standing on Base, smooth sides and flat caps. */
		void AddCylinder(FPolygonGroupID Group, const FVector& Base, double Radius, double Height, int32 Sides)
		{
			const FVector Up(0.0, 0.0, Height);
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const double Angle0 = 2.0 * UE_DOUBLE_PI * Side / Sides;
				const double Angle1 = 2.0 * UE_DOUBLE_PI * (Side + 1) / Sides;
				const FVector Dir0(FMath::Cos(Angle0), FMath::Sin(Angle0), 0.0);
				const FVector Dir1(FMath::Cos(Angle1), FMath::Sin(Angle1), 0.0);
				const FVector Bottom0 = Base + Dir0 * Radius;
				const FVector Bottom1 = Base + Dir1 * Radius;

				AddTriangle(Group, Bottom0, Bottom1, Bottom1 + Up, Dir0, Dir1, Dir1);
				AddTriangle(Group, Bottom0, Bottom1 + Up, Bottom0 + Up, Dir0, Dir1, Dir0);
				AddTriangle(Group, Base + Up, Bottom0 + Up, Bottom1 + Up, FVector::UpVector, FVector::UpVector, FVector::UpVector);
				AddTriangle(Group, Base, Bottom1, Bottom0, FVector::DownVector, FVector::DownVector, FVector::DownVector);
			}
		}

		/** Ellipsoid centred on Center, with smooth normals. */
		void AddEllipsoid(FPolygonGroupID Group, const FVector& Center, const FVector& Radii, int32 Rings, int32 Segments)
		{
			auto Direction = [](double Phi, double Theta)
			{
				return FVector(FMath::Sin(Phi) * FMath::Cos(Theta), FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Phi));
			};
			auto Position = [&Center, &Radii](const FVector& Dir) { return Center + Dir * Radii; };
			auto Normal = [&Radii](const FVector& Dir) { return (Dir / Radii).GetSafeNormal(); };

			for (int32 Ring = 0; Ring < Rings; ++Ring)
			{
				const double Phi0 = UE_DOUBLE_PI * Ring / Rings;
				const double Phi1 = UE_DOUBLE_PI * (Ring + 1) / Rings;
				for (int32 Segment = 0; Segment < Segments; ++Segment)
				{
					const double Theta0 = 2.0 * UE_DOUBLE_PI * Segment / Segments;
					const double Theta1 = 2.0 * UE_DOUBLE_PI * (Segment + 1) / Segments;
					const FVector D00 = Direction(Phi0, Theta0);
					const FVector D01 = Direction(Phi0, Theta1);
					const FVector D10 = Direction(Phi1, Theta0);
					const FVector D11 = Direction(Phi1, Theta1);

					AddTriangle(Group, Position(D00), Position(D10), Position(D11), Normal(D00), Normal(D10), Normal(D11));
					AddTriangle(Group, Position(D00), Position(D11), Position(D01), Normal(D00), Normal(D11), Normal(D01));
				}
			}
		}

	private:
		void AddTriangle(FPolygonGroupID Group, FVector A, FVector B, FVector C, FVector NormalA, FVector NormalB, FVector NormalC)
		{
			// Unreal draws the side whose normal is (C - A) x (B - A): if that side looks inwards, swap two corners
			const FVector FaceNormal = FVector::CrossProduct(C - A, B - A);
			if (FaceNormal.SizeSquared() < UE_SMALL_NUMBER)
			{
				return; // degenerate (the poles of the ellipsoid)
			}
			if (FVector::DotProduct(FaceNormal, NormalA + NormalB + NormalC) < 0.0)
			{
				Swap(B, C);
				Swap(NormalB, NormalC);
			}

			const FVector Corners[3] = { A, B, C };
			const FVector Normals[3] = { NormalA, NormalB, NormalC };
			TArray<FVertexInstanceID, TInlineAllocator<3>> Instances;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FVertexID Vertex = MeshDescription.CreateVertex();
				Attributes.GetVertexPositions()[Vertex] = FVector3f(Corners[Index]);

				const FVertexInstanceID Instance = MeshDescription.CreateVertexInstance(Vertex);
				Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normals[Index].GetSafeNormal());
				Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(
					static_cast<float>(Corners[Index].X / 100.0),
					static_cast<float>((Corners[Index].Y + Corners[Index].Z) / 100.0)));
				Instances.Add(Instance);
			}
			MeshDescription.CreateTriangle(Group, Instances);
		}
	};

	FString ListProperties(const UStruct* Struct)
	{
		TArray<FString> Names;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			Names.Add(It->GetName());
		}
		return FString::Join(Names, TEXT(", "));
	}

	FString ListPins(const TArray<TObjectPtr<UPCGPin>>& Pins)
	{
		TArray<FString> Labels;
		for (const UPCGPin* Pin : Pins)
		{
			if (Pin)
			{
				Labels.Add(Pin->Properties.Label.ToString());
			}
		}
		return FString::Join(Labels, TEXT(", "));
	}

	/**
	 * Writes nodes and edges into a PCG graph. The standard PCG nodes are found by class name and
	 * their properties are set by reflection, from text, exactly as Copy/Paste does in Details: the
	 * code does not depend on their headers, and a property that does not exist in this engine
	 * version is reported with the list of the ones that do, instead of breaking the build.
	 */
	class FGraphWriter
	{
	public:
		/** One stage of a chain of nodes: the pin the data comes in through and the one it leaves through. */
		struct FStage
		{
			UPCGNode* Node;
			FName InPin;
			FName OutPin;
		};

		FGraphWriter(UPCGGraph* InGraph, TFunction<void(const FString&)> InOnError)
			: Graph(InGraph)
			, OnError(MoveTemp(InOnError))
		{
		}

		UPCGGraph* GetGraph() const { return Graph; }

		UPCGNode* Add(UClass* SettingsClass, int32 X, int32 Y)
		{
			UPCGSettings* Settings = nullptr;
			UPCGNode* Node = SettingsClass ? Graph->AddNodeOfType(SettingsClass, Settings) : nullptr;
			if (!Node)
			{
				Fail(FString::Printf(TEXT("no se ha podido crear un nodo %s."), SettingsClass ? *SettingsClass->GetName() : TEXT("(sin clase)")));
				return nullptr;
			}
			SetPosition(Node, X, Y);
			return Node;
		}

		/** A standard PCG node, by the name of its settings class (PCGDensityFilterSettings...). */
		UPCGNode* AddStandard(const TCHAR* ClassName, int32 X, int32 Y)
		{
			UClass* Class = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/PCG.%s"), ClassName));
			if (!Class || !Class->IsChildOf(UPCGSettings::StaticClass()))
			{
				Fail(FString::Printf(TEXT("no existe el nodo PCG %s en esta versión del motor."), ClassName));
				return nullptr;
			}
			return Add(Class, X, Y);
		}

		void SetPosition(UPCGNode* Node, int32 X, int32 Y)
		{
			if (!Node)
			{
				return;
			}
			if (FIntProperty* PositionX = FindFProperty<FIntProperty>(UPCGNode::StaticClass(), TEXT("PositionX")))
			{
				PositionX->SetPropertyValue_InContainer(Node, X);
			}
			if (FIntProperty* PositionY = FindFProperty<FIntProperty>(UPCGNode::StaticClass(), TEXT("PositionY")))
			{
				PositionY->SetPropertyValue_InContainer(Node, Y);
			}
		}

		/** Sets a property of the node settings from text. The path can go into structs and sub-objects ("A.B"). */
		void Set(UPCGNode* Node, const TCHAR* Path, const FString& Value)
		{
			UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
			if (!Settings)
			{
				return;
			}

			TArray<FString> Parts;
			FString(Path).ParseIntoArray(Parts, TEXT("."));
			UObject* Owner = Settings;
			const UStruct* Struct = Settings->GetClass();
			void* Container = Settings;

			for (int32 Index = 0; Index < Parts.Num(); ++Index)
			{
				FProperty* Property = Struct->FindPropertyByName(FName(*Parts[Index]));
				if (!Property)
				{
					Fail(FString::Printf(TEXT("%s no tiene la propiedad '%s'. Tiene: %s"), *Struct->GetName(), *Parts[Index], *ListProperties(Struct)));
					return;
				}

				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
				if (Index == Parts.Num() - 1)
				{
					if (!Property->ImportText_Direct(*Value, ValuePtr, Owner, PPF_None))
					{
						Fail(FString::Printf(TEXT("no se puede poner '%s' en %s.%s."), *Value, *Struct->GetName(), *Parts[Index]));
					}
					return;
				}

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					Struct = StructProperty->Struct;
					Container = ValuePtr;
				}
				else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					UObject* Child = ObjectProperty->GetObjectPropertyValue(ValuePtr);
					if (!Child)
					{
						Fail(FString::Printf(TEXT("%s.%s está vacío."), *Struct->GetName(), *Parts[Index]));
						return;
					}
					Owner = Child;
					Struct = Child->GetClass();
					Container = Child;
				}
				else
				{
					Fail(FString::Printf(TEXT("%s.%s no es una estructura ni un objeto."), *Struct->GetName(), *Parts[Index]));
					return;
				}
			}
		}

		/** Lets the node rebuild its pins after its settings changed. */
		void Refresh(UPCGNode* Node)
		{
			if (UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr)
			{
				static_cast<UObject*>(Settings)->PostEditChange();
			}
		}

		void Connect(UPCGNode* From, FName FromPin, UPCGNode* To, FName ToPin)
		{
			if (!From || !To)
			{
				return;
			}
			if (!From->GetOutputPin(FromPin))
			{
				Fail(FString::Printf(TEXT("%s no tiene la salida '%s'. Tiene: %s"), *From->GetSettings()->GetClass()->GetName(), *FromPin.ToString(), *ListPins(From->GetOutputPins())));
				return;
			}
			if (!To->GetInputPin(ToPin))
			{
				Fail(FString::Printf(TEXT("%s no tiene la entrada '%s'. Tiene: %s"), *To->GetSettings()->GetClass()->GetName(), *ToPin.ToString(), *ListPins(To->GetInputPins())));
				return;
			}
			Graph->AddEdge(From, FromPin, To, ToPin);
		}

		/**
		 * Connects From to each stage in turn. A stage whose node could not be created is skipped and
		 * the chain goes on with the next one, so a missing standard node never breaks the rest.
		 */
		void Chain(UPCGNode* From, FName FromPin, std::initializer_list<FStage> Stages)
		{
			for (const FStage& Stage : Stages)
			{
				if (!Stage.Node)
				{
					continue;
				}
				Connect(From, FromPin, Stage.Node, Stage.InPin);
				From = Stage.Node;
				FromPin = Stage.OutPin;
			}
		}

		/** Adds a Graph Parameter of type Double, or just sets its default value if it already exists. */
		void AddParameter(FName Name, double DefaultValue)
		{
			const FInstancedPropertyBag* Existing = Graph->GetUserParametersStruct();
			if (!Existing || !Existing->FindPropertyDescByName(Name))
			{
				TArray<FPropertyBagPropertyDesc> NewParameters;
				NewParameters.Emplace(Name, EPropertyBagPropertyType::Double);
				Graph->AddUserParameters(NewParameters);
			}

			FInstancedPropertyBag* Values = const_cast<FInstancedPropertyBag*>(Graph->GetUserParametersStruct());
			if (!Values || Values->SetValueDouble(Name, DefaultValue) != EPropertyBagResult::Success)
			{
				Fail(FString::Printf(TEXT("no se ha podido crear el parámetro %s."), *Name.ToString()));
			}
		}

		/** A Get Graph Parameter node for one of the graph's parameters. */
		UPCGNode* AddGetParameter(FName Name, int32 X, int32 Y)
		{
			const FInstancedPropertyBag* Parameters = Graph->GetUserParametersStruct();
			const FPropertyBagPropertyDesc* Desc = Parameters ? Parameters->FindPropertyDescByName(Name) : nullptr;
			if (!Desc)
			{
				Fail(FString::Printf(TEXT("el grafo no tiene el parámetro %s."), *Name.ToString()));
				return nullptr;
			}

			UPCGNode* Node = AddStandard(TEXT("PCGUserParameterGetSettings"), X, Y);
			Set(Node, TEXT("PropertyGuid"), Desc->ID.ToString());
			Set(Node, TEXT("PropertyName"), Name.ToString());
			Refresh(Node);
			return Node;
		}

		/** Connects the output of a Get Graph Parameter node (it has a single one) to an input pin. */
		void ConnectParameter(UPCGNode* GetNode, UPCGNode* To, FName ToPin)
		{
			if (!GetNode || GetNode->GetOutputPins().IsEmpty() || !GetNode->GetOutputPins()[0])
			{
				Fail(FString::Printf(TEXT("el nodo del parámetro para %s no tiene salida."), *ToPin.ToString()));
				return;
			}
			Connect(GetNode, GetNode->GetOutputPins()[0]->Properties.Label, To, ToPin);
		}

		/** Makes a Subgraph node run the given graph. Its pins become the subgraph's Graph Parameters. */
		void SetSubgraph(UPCGNode* Node, UPCGGraph* Subgraph)
		{
			UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
			if (!Settings || !Subgraph)
			{
				return;
			}
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Settings->GetClass(), TEXT("SubgraphInstance"));
			UPCGGraphInstance* Instance = Property ? Cast<UPCGGraphInstance>(Property->GetObjectPropertyValue_InContainer(Settings)) : nullptr;
			if (!Instance)
			{
				Fail(FString::Printf(TEXT("%s no tiene SubgraphInstance. Tiene: %s"), *Settings->GetClass()->GetName(), *ListProperties(Settings->GetClass())));
				return;
			}
			Instance->SetGraph(Subgraph);

			FPropertyChangedEvent Event(Property);
			static_cast<UObject*>(Settings)->PostEditChangeProperty(Event);
		}

		/** Leaves the graph as a new one: only its Input and Output nodes. Graph Parameters are kept. */
		void Clear()
		{
			TArray<UPCGNode*> Nodes;
			for (UPCGNode* Node : Graph->GetNodes())
			{
				Nodes.Add(Node);
			}
			for (UPCGNode* Node : Nodes)
			{
				Graph->RemoveNode(Node);
			}
		}

	private:
		void Fail(const FString& Message)
		{
			OnError(FString::Printf(TEXT("%s: %s"), *Graph->GetName(), *Message));
		}

		UPCGGraph* Graph;
		TFunction<void(const FString&)> OnError;
	};

	/** Text for the Mesh Entries of a Static Mesh Spawner: one entry per mesh, with its weight and the mesh's own materials. */
	FString WeightedMeshEntriesText(const TArray<TPair<FString, int32>>& MeshesAndWeights)
	{
		TArray<FString> Entries;
		for (const TPair<FString, int32>& Mesh : MeshesAndWeights)
		{
			Entries.Add(FString::Printf(TEXT("(Descriptor=(StaticMesh=\"%s\"),Weight=%d)"), *Mesh.Key, Mesh.Value));
		}
		return TEXT("(") + FString::Join(Entries, TEXT(",")) + TEXT(")");
	}

	/** Text for the Mesh Entries of a Static Mesh Spawner: one entry per (mesh, material), weight 1. */
	FString MeshEntriesText(const FString& MeshPath, const TArray<FString>& MaterialPaths)
	{
		TArray<FString> Entries;
		if (MaterialPaths.IsEmpty())
		{
			Entries.Add(FString::Printf(TEXT("(Descriptor=(StaticMesh=\"%s\"),Weight=1)"), *MeshPath));
		}
		for (const FString& MaterialPath : MaterialPaths)
		{
			Entries.Add(FString::Printf(TEXT("(Descriptor=(StaticMesh=\"%s\",OverrideMaterials=(\"%s\")),Weight=1)"), *MeshPath, *MaterialPath));
		}
		return TEXT("(") + FString::Join(Entries, TEXT(",")) + TEXT(")");
	}
}

FString FCityContent::CityGraphObjectPath()
{
	return ObjectPathOf(CityGraphPackage());
}

FString FCityContent::FReport::ToString() const
{
	if (IsEmpty())
	{
		return TEXT("El contenido del plugin está al día.");
	}

	FString Text = FString::Printf(TEXT("Contenido: %d creados, %d grafos rehechos, %d errores."), Created.Num(), Rebuilt.Num(), Errors.Num());
	for (const FString& Message : Errors)
	{
		Text += TEXT("\n- ") + Message;
	}
	return Text;
}

FCityContent::FReport FCityContent::Ensure(bool bRebuildGraphs)
{
	FReport Report;
	FCityContent Content(Report);
	Content.BuildMaterials();
	Content.BuildMeshes();
	if (bRebuildGraphs || !Content.AreGraphsUpToDate())
	{
		Content.BuildGraphs();
	}
	Content.SaveAll();
	return Report;
}

void FCityContent::Error(const FString& Message)
{
	UE_LOG(LogCityContent, Warning, TEXT("%s"), *Message);
	Report.Errors.Add(Message);
}

template <typename T>
T* FCityContent::FindExisting(const FString& PackageName) const
{
	const FString ObjectPath = ObjectPathOf(PackageName);
	if (T* InMemory = FindObject<T>(nullptr, *ObjectPath))
	{
		if (IsValid(InMemory))
		{
			return InMemory;
		}
	}
	return FPackageName::DoesPackageExist(PackageName) ? LoadObject<T>(nullptr, *ObjectPath) : nullptr;
}

template <typename T>
T* FCityContent::CreateAsset(const FString& PackageName)
{
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	T* Asset = NewObject<T>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();

	PackagesToSave.AddUnique(Package);
	Report.Created.Add(PackageName);
	return Asset;
}

void FCityContent::BuildMaterials()
{
	// Base material: one colour parameter and one roughness parameter, nothing else
	const FString BasePackage = MaterialsPath() / TEXT("M_CityColor");
	UMaterial* BaseMaterial = FindExisting<UMaterial>(BasePackage);
	if (!BaseMaterial)
	{
		BaseMaterial = CreateAsset<UMaterial>(BasePackage);

		UMaterialExpressionVectorParameter* Color = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(BaseMaterial, UMaterialExpressionVectorParameter::StaticClass(), -400, 0));
		Color->ParameterName = TEXT("Color");
		Color->DefaultValue = FLinearColor(0.5f, 0.5f, 0.5f);
		UMaterialEditingLibrary::ConnectMaterialProperty(Color, FString(), MP_BaseColor);

		UMaterialExpressionScalarParameter* Roughness = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(BaseMaterial, UMaterialExpressionScalarParameter::StaticClass(), -400, 250));
		Roughness->ParameterName = TEXT("Roughness");
		Roughness->DefaultValue = 0.8f;
		UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, FString(), MP_Roughness);

		UMaterialEditingLibrary::RecompileMaterial(BaseMaterial);
	}

	// One instance per colour
	for (const FColorDefinition& Definition : MaterialColors)
	{
		const FString PackageName = MaterialsPath() / Definition.Name;
		if (FindExisting<UMaterialInstanceConstant>(PackageName))
		{
			continue;
		}

		UMaterialInstanceConstant* Instance = CreateAsset<UMaterialInstanceConstant>(PackageName);
		UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, BaseMaterial);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("Color"), Definition.Color);
		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	}
}

void FCityContent::BuildMeshes()
{
	struct FSlot
	{
		FName Name;
		const TCHAR* Material;
	};

	auto BuildMesh = [this](const TCHAR* AssetName, TArrayView<const FSlot> Slots, TFunctionRef<void(FSimpleMeshBuilder&, TArrayView<const FPolygonGroupID>)> AddSolids)
	{
		const FString PackageName = MeshesPath() / AssetName;
		if (FindExisting<UStaticMesh>(PackageName))
		{
			return;
		}

		FSimpleMeshBuilder MeshBuilder;
		TArray<FPolygonGroupID> Groups;
		for (const FSlot& Slot : Slots)
		{
			Groups.Add(MeshBuilder.AddSlot(Slot.Name));
		}
		AddSolids(MeshBuilder, Groups);

		UStaticMesh* Mesh = CreateAsset<UStaticMesh>(PackageName);
		for (const FSlot& Slot : Slots)
		{
			UMaterialInterface* Material = FindExisting<UMaterialInterface>(MaterialsPath() / Slot.Material);
			if (!Material)
			{
				Error(FString::Printf(TEXT("%s: falta el material %s."), AssetName, Slot.Material));
			}
			Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, Slot.Name, Slot.Name));
		}

		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = true; // keep the geometry in the asset so it can be saved
		Params.bMarkPackageDirty = true;
		TArray<const FMeshDescription*> Descriptions;
		Descriptions.Add(&MeshBuilder.MeshDescription);
		if (!Mesh->BuildFromMeshDescriptions(Descriptions, Params))
		{
			Error(FString::Printf(TEXT("%s: no se ha podido construir la malla."), AssetName));
		}
		Mesh->PostEditChange();
	};

	// Street lamp: a 4.5 m post and a light on top
	const FSlot LampSlots[] = { { TEXT("Post"), TEXT("MI_LampPost") }, { TEXT("Light"), TEXT("MI_LampLight") } };
	BuildMesh(TEXT("SM_StreetLamp"), LampSlots, [](FSimpleMeshBuilder& Mesh, TArrayView<const FPolygonGroupID> Groups)
	{
		Mesh.AddCylinder(Groups[0], FVector::ZeroVector, 6.0, 450.0, 10);
		Mesh.AddEllipsoid(Groups[1], FVector(0.0, 0.0, 460.0), FVector(25.0, 25.0, 18.0), 6, 12);
	});

	// Tree: a 2 m trunk and a round crown about 1.1 m in radius, 4.4 m tall in total
	const FSlot TreeSlots[] = { { TEXT("Trunk"), TEXT("MI_TreeTrunk") }, { TEXT("Leaves"), TEXT("MI_TreeLeaves") } };
	BuildMesh(TEXT("SM_Tree"), TreeSlots, [](FSimpleMeshBuilder& Mesh, TArrayView<const FPolygonGroupID> Groups)
	{
		Mesh.AddCylinder(Groups[0], FVector::ZeroVector, 10.0, 200.0, 8);
		Mesh.AddEllipsoid(Groups[1], FVector(0.0, 0.0, 310.0), FVector(110.0, 110.0, 130.0), 8, 12);
	});

	// Bush: a low, wide ellipsoid, 1.2 m tall
	const FSlot BushSlots[] = { { TEXT("Leaves"), TEXT("MI_Bush") } };
	BuildMesh(TEXT("SM_Bush"), BushSlots, [](FSimpleMeshBuilder& Mesh, TArrayView<const FPolygonGroupID> Groups)
	{
		Mesh.AddEllipsoid(Groups[0], FVector(0.0, 0.0, 55.0), FVector(90.0, 90.0, 65.0), 6, 12);
	});
}

bool FCityContent::AreGraphsUpToDate() const
{
	for (const TCHAR* Name : SubgraphNames)
	{
		if (!FindExisting<UPCGGraph>(SubgraphPackage(Name)))
		{
			return false;
		}
	}

	UPCGGraph* City = FindExisting<UPCGGraph>(CityGraphPackage());
	const FInstancedPropertyBag* Parameters = City ? City->GetUserParametersStruct() : nullptr;
	if (!Parameters)
	{
		return false;
	}
	const TValueOrError<double, EPropertyBagResult> Version = Parameters->GetValueDouble(CityParamNames::GraphVersion);
	return Version.HasValue() && FMath::IsNearlyEqual(Version.GetValue(), static_cast<double>(GraphVersion));
}

void FCityContent::BuildGraphs()
{
	using namespace CityPCGNames;
	namespace Param = CityParamNames;

	const int32 ErrorsBefore = Report.Errors.Num();
	const FName In = PCGPinConstants::DefaultInputLabel;
	const FName Out = PCGPinConstants::DefaultOutputLabel;
	const FString Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const TFunction<void(const FString&)> OnError = [this](const FString& Message) { Error(Message); };

	auto MaterialPath = [](const TCHAR* Name) { return ObjectPathOf(MaterialsPath() / Name); };
	auto MeshPath = [](const TCHAR* Name) { return ObjectPathOf(MeshesPath() / Name); };

	// A graph ready to be filled: a new asset, or the existing one emptied (same asset, same references)
	auto StartGraph = [this, &OnError](const FString& PackageName)
	{
		UPCGGraph* Graph = FindExisting<UPCGGraph>(PackageName);
		if (!Graph)
		{
			return CreateAsset<UPCGGraph>(PackageName);
		}
		FGraphWriter(Graph, OnError).Clear();
		Graph->MarkPackageDirty();
		PackagesToSave.AddUnique(Graph->GetPackage());
		Report.Rebuilt.Add(PackageName);
		return Graph;
	};

	auto PlaceInputOutput = [](FGraphWriter& Writer, int32 InputX, int32 OutputX)
	{
		Writer.SetPosition(Writer.GetGraph()->GetInputNode(), InputX, 0);
		Writer.SetPosition(Writer.GetGraph()->GetOutputNode(), OutputX, 0);
	};

	auto AddSpawner = [](FGraphWriter& Writer, int32 X, int32 Y, const FString& EntriesText)
	{
		UPCGNode* Spawner = Writer.AddStandard(TEXT("PCGStaticMeshSpawnerSettings"), X, Y);
		Writer.Set(Spawner, TEXT("MeshSelectorParameters.MeshEntries"), EntriesText);
		Writer.Refresh(Spawner);
		return Spawner;
	};

	// Every subgraph starts the same way: CitySize and BlockSize into a City Street Grid node.
	// The subgraphs that use UrbanBlocks or ParkBlocks also need ParkRatio, so all of them agree.
	auto AddStreetGrid = [](FGraphWriter& Writer, bool bUsesParks = false)
	{
		Writer.AddParameter(Param::CitySize, 400.0);
		Writer.AddParameter(Param::BlockSize, 60.0);
		UPCGNode* GetCitySize = Writer.AddGetParameter(Param::CitySize, -1100, -100);
		UPCGNode* GetBlockSize = Writer.AddGetParameter(Param::BlockSize, -1100, 100);
		UPCGNode* Grid = Writer.Add(UPCGCityStreetGridSettings::StaticClass(), -750, 0);
		Writer.ConnectParameter(GetCitySize, Grid, Param::CitySize);
		Writer.ConnectParameter(GetBlockSize, Grid, Param::BlockSize);
		if (bUsesParks)
		{
			Writer.AddParameter(Param::ParkRatio, 0.15);
			Writer.ConnectParameter(Writer.AddGetParameter(Param::ParkRatio, -1100, 250), Grid, Param::ParkRatio);
		}
		return Grid;
	};

	auto AddSidewalkPoints = [](FGraphWriter& Writer, double Spacing, double Phase, double CurbOffset, double CornerClearance)
	{
		UPCGNode* Node = Writer.Add(UPCGCitySidewalkPointsSettings::StaticClass(), -400, 0);
		if (UPCGCitySidewalkPointsSettings* Settings = Node ? Cast<UPCGCitySidewalkPointsSettings>(Node->GetSettings()) : nullptr)
		{
			Settings->Spacing = Spacing;
			Settings->Phase = Phase;
			Settings->CurbOffset = CurbOffset;
			Settings->CornerClearance = CornerClearance;
		}
		return Node;
	};

	// --- SG_Streets: the road slabs ---------------------------------------------------------------
	UPCGGraph* Streets = StartGraph(SubgraphPackage(TEXT("SG_Streets")));
	{
		FGraphWriter Writer(Streets, OnError);
		PlaceInputOutput(Writer, -1400, 400);
		UPCGNode* Grid = AddStreetGrid(Writer);
		UPCGNode* Spawner = AddSpawner(Writer, -350, 0, MeshEntriesText(Cube, { MaterialPath(TEXT("MI_Road")) }));
		Writer.Chain(Grid, RoadsPin, { { Spawner, In, Out } });
	}

	// --- SG_Blocks: the blocks, drawn as the sidewalk slab -----------------------------------------
	UPCGGraph* Blocks = StartGraph(SubgraphPackage(TEXT("SG_Blocks")));
	{
		FGraphWriter Writer(Blocks, OnError);
		PlaceInputOutput(Writer, -1400, 400);
		UPCGNode* Grid = AddStreetGrid(Writer);
		UPCGNode* Spawner = AddSpawner(Writer, -350, 0, MeshEntriesText(Cube, { MaterialPath(TEXT("MI_Sidewalk")) }));
		Writer.Chain(Grid, BlocksPin, { { Spawner, In, Out } });
	}

	// --- SG_Buildings: lots -> density -> buildings -> rotation -> pruning -> meshes ---------------
	UPCGGraph* Buildings = StartGraph(SubgraphPackage(TEXT("SG_Buildings")));
	{
		FGraphWriter Writer(Buildings, OnError);
		PlaceInputOutput(Writer, -1400, 1800);
		UPCGNode* Grid = AddStreetGrid(Writer, /*bUsesParks=*/true);
		Writer.AddParameter(Param::BuildingDensity, 0.6);
		Writer.AddParameter(Param::BuildingHeight, 30.0);
		Writer.AddParameter(Param::BuildingVariation, 0.5);

		UPCGNode* GetDensity = Writer.AddGetParameter(Param::BuildingDensity, -750, 300);
		UPCGNode* Lots = Writer.Add(UPCGCityLotsSettings::StaticClass(), -400, 0);

		UPCGNode* DensityFilter = Writer.AddStandard(TEXT("PCGDensityFilterSettings"), -50, 0);
		Writer.Set(DensityFilter, TEXT("LowerBound"), TEXT("0.5"));
		Writer.Set(DensityFilter, TEXT("UpperBound"), TEXT("1.0"));
		Writer.Refresh(DensityFilter);

		UPCGNode* GetHeight = Writer.AddGetParameter(Param::BuildingHeight, -50, 250);
		UPCGNode* GetVariation = Writer.AddGetParameter(Param::BuildingVariation, -50, 400);
		UPCGNode* CityBuildings = Writer.Add(UPCGCityBuildingsSettings::StaticClass(), 300, 0);

		// A few degrees of yaw per building, from the seed
		UPCGNode* Rotation = Writer.AddStandard(TEXT("PCGTransformPointsSettings"), 650, 0);
		Writer.Set(Rotation, TEXT("RotationMin"), TEXT("(Pitch=0.0,Yaw=-4.0,Roll=0.0)"));
		Writer.Set(Rotation, TEXT("RotationMax"), TEXT("(Pitch=0.0,Yaw=4.0,Roll=0.0)"));
		Writer.Refresh(Rotation);

		UPCGNode* Pruning = Writer.AddStandard(TEXT("PCGSelfPruningSettings"), 1000, 0);
		UPCGNode* Spawner = AddSpawner(Writer, 1350, 0, MeshEntriesText(Cube,
			{ MaterialPath(TEXT("MI_Building_A")), MaterialPath(TEXT("MI_Building_B")), MaterialPath(TEXT("MI_Building_C")) }));

		Writer.Connect(Grid, UrbanBlocksPin, Lots, BlocksPin); // parks get no buildings
		Writer.ConnectParameter(GetDensity, Lots, Param::BuildingDensity);
		Writer.ConnectParameter(GetHeight, CityBuildings, Param::BuildingHeight);
		Writer.ConnectParameter(GetVariation, CityBuildings, Param::BuildingVariation);
		Writer.Chain(Lots, LotsPin, {
			{ DensityFilter, In, Out },
			{ CityBuildings, LotsPin, BuildingsPin },
			{ Rotation, In, Out },
			{ Pruning, In, Out },
			{ Spawner, In, Out } });
	}

	// --- SG_StreetLamps: near the kerb, every 24 m ---------------------------------------------------
	UPCGGraph* StreetLamps = StartGraph(SubgraphPackage(TEXT("SG_StreetLamps")));
	{
		FGraphWriter Writer(StreetLamps, OnError);
		PlaceInputOutput(Writer, -1400, 400);
		UPCGNode* Grid = AddStreetGrid(Writer);
		UPCGNode* Points = AddSidewalkPoints(Writer, 24.0, 0.0, 0.6, 6.0);
		UPCGNode* Spawner = AddSpawner(Writer, 0, 0, MeshEntriesText(MeshPath(TEXT("SM_StreetLamp")), {}));
		Writer.Connect(Grid, BlocksPin, Points, BlocksPin);
		Writer.Chain(Points, PointsPin, { { Spawner, In, Out } });
	}

	// --- SG_Trees: halfway between the lamps, with random yaw and size ------------------------------
	UPCGGraph* Trees = StartGraph(SubgraphPackage(TEXT("SG_Trees")));
	{
		FGraphWriter Writer(Trees, OnError);
		PlaceInputOutput(Writer, -1400, 750);
		UPCGNode* Grid = AddStreetGrid(Writer);
		UPCGNode* Points = AddSidewalkPoints(Writer, 24.0, 0.5, 1.4, 6.0);

		UPCGNode* Variation = Writer.AddStandard(TEXT("PCGTransformPointsSettings"), 0, 0);
		Writer.Set(Variation, TEXT("RotationMax"), TEXT("(Pitch=0.0,Yaw=360.0,Roll=0.0)"));
		Writer.Set(Variation, TEXT("ScaleMin"), TEXT("(X=0.8,Y=0.8,Z=0.8)"));
		Writer.Set(Variation, TEXT("ScaleMax"), TEXT("(X=1.25,Y=1.25,Z=1.25)"));
		Writer.Set(Variation, TEXT("bUniformScale"), TEXT("True"));
		Writer.Refresh(Variation);

		UPCGNode* Spawner = AddSpawner(Writer, 350, 0, MeshEntriesText(MeshPath(TEXT("SM_Tree")), {}));
		Writer.Connect(Grid, BlocksPin, Points, BlocksPin);
		Writer.Chain(Points, PointsPin, { { Variation, In, Out }, { Spawner, In, Out } });
	}

	// --- SG_Parks: lawn, trees and bushes on the park blocks -------------------------------------------
	UPCGGraph* Parks = StartGraph(SubgraphPackage(TEXT("SG_Parks")));
	{
		FGraphWriter Writer(Parks, OnError);
		PlaceInputOutput(Writer, -1400, 1100);
		UPCGNode* Grid = AddStreetGrid(Writer, /*bUsesParks=*/true);
		Writer.AddParameter(Param::ParkTreeDensity, 0.6);
		Writer.AddParameter(Param::ParkTreeScaleVariation, 0.4);

		UPCGNode* GetTreeDensity = Writer.AddGetParameter(Param::ParkTreeDensity, -750, 300);
		UPCGNode* GetScaleVariation = Writer.AddGetParameter(Param::ParkTreeScaleVariation, -750, 450);
		UPCGNode* ParkNode = Writer.Add(UPCGCityParkSettings::StaticClass(), -400, 0);
		UPCGNode* LawnSpawner = AddSpawner(Writer, 0, -200, MeshEntriesText(Cube, { MaterialPath(TEXT("MI_Grass")) }));

		// Random yaw for every plant; its size already comes from City Park
		UPCGNode* Rotation = Writer.AddStandard(TEXT("PCGTransformPointsSettings"), 0, 150);
		Writer.Set(Rotation, TEXT("RotationMax"), TEXT("(Pitch=0.0,Yaw=360.0,Roll=0.0)"));
		Writer.Refresh(Rotation);

		// Two trees for every bush, chosen from the seed
		TArray<TPair<FString, int32>> PlantMeshes;
		PlantMeshes.Emplace(MeshPath(TEXT("SM_Tree")), 2);
		PlantMeshes.Emplace(MeshPath(TEXT("SM_Bush")), 1);
		UPCGNode* PlantSpawner = AddSpawner(Writer, 350, 150, WeightedMeshEntriesText(PlantMeshes));

		Writer.Connect(Grid, ParkBlocksPin, ParkNode, BlocksPin);
		Writer.ConnectParameter(GetTreeDensity, ParkNode, TEXT("TreeDensity"));
		Writer.ConnectParameter(GetScaleVariation, ParkNode, TEXT("ScaleVariation"));
		Writer.Chain(ParkNode, LawnsPin, { { LawnSpawner, In, Out } });
		Writer.Chain(ParkNode, PlantsPin, { { Rotation, In, Out }, { PlantSpawner, In, Out } });
	}

	// --- PCG_City: only runs the subgraphs and passes the parameters down -------------------------
	UPCGGraph* City = StartGraph(CityGraphPackage());
	{
		FGraphWriter Writer(City, OnError);
		PlaceInputOutput(Writer, -1100, 800);
		Writer.AddParameter(Param::CitySize, 400.0);
		Writer.AddParameter(Param::BlockSize, 60.0);
		Writer.AddParameter(Param::BuildingDensity, 0.6);
		Writer.AddParameter(Param::BuildingHeight, 30.0);
		Writer.AddParameter(Param::BuildingVariation, 0.5);
		Writer.AddParameter(Param::ParkRatio, 0.15);
		Writer.AddParameter(Param::ParkTreeDensity, 0.6);
		Writer.AddParameter(Param::ParkTreeScaleVariation, 0.4);
		Writer.AddParameter(Param::GraphVersion, 0.0); // the real version is written at the end, only if all went well

		// One Get Graph Parameter node per parameter, shared by every subgraph that needs it
		const FName AllParams[] = { Param::CitySize, Param::BlockSize, Param::BuildingDensity, Param::BuildingHeight,
			Param::BuildingVariation, Param::ParkRatio, Param::ParkTreeDensity, Param::ParkTreeScaleVariation };
		TMap<FName, UPCGNode*> GetNodes;
		int32 GetY = -400;
		for (const FName& Name : AllParams)
		{
			GetNodes.Add(Name, Writer.AddGetParameter(Name, -800, GetY));
			GetY += 150;
		}

		// Each subgraph and the parameters it receives (they are the Graph Parameters it has)
		struct FSystem
		{
			UPCGGraph* Graph;
			TArray<FName> Params;
		};
		const FSystem Systems[] =
		{
			{ Streets, { Param::CitySize, Param::BlockSize } },
			{ Blocks, { Param::CitySize, Param::BlockSize } },
			{ Buildings, { Param::CitySize, Param::BlockSize, Param::ParkRatio, Param::BuildingDensity, Param::BuildingHeight, Param::BuildingVariation } },
			{ StreetLamps, { Param::CitySize, Param::BlockSize } },
			{ Trees, { Param::CitySize, Param::BlockSize } },
			{ Parks, { Param::CitySize, Param::BlockSize, Param::ParkRatio, Param::ParkTreeDensity, Param::ParkTreeScaleVariation } },
		};

		int32 Y = -400;
		for (const FSystem& System : Systems)
		{
			UPCGNode* Node = Writer.AddStandard(TEXT("PCGSubgraphSettings"), 300, Y);
			Y += 200;
			Writer.SetSubgraph(Node, System.Graph);
			for (const FName& Name : System.Params)
			{
				Writer.ConnectParameter(GetNodes.FindRef(Name), Node, Name);
			}
		}

		if (Report.Errors.Num() == ErrorsBefore)
		{
			Writer.AddParameter(Param::GraphVersion, static_cast<double>(GraphVersion));
		}
	}
}

void FCityContent::SaveAll()
{
	if (PackagesToSave.IsEmpty())
	{
		return;
	}
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty=*/false))
	{
		Error(TEXT("No se han podido guardar todos los assets. Usa File > Save All."));
	}
}
