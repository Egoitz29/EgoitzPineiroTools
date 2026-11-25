#include "CityContentBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CityPCG/CityPCGNames.h"
#include "CityPCG/PCGCityBlocks.h"
#include "CityPCG/PCGCityBuildings.h"
#include "CityPCG/PCGCityCrossings.h"
#include "CityPCG/PCGCityLots.h"
#include "CityPCG/PCGCitySidewalkSlots.h"
#include "CityPCG/PCGCityStreetSegments.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Misc/PackageName.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityContent, Log, All);

#define LOCTEXT_NAMESPACE "CityContentBuilder"

const TCHAR* FCityContentBuilder::RootPath = TEXT("/EgoitzPineiroTools/City");
const TCHAR* FCityContentBuilder::MaterialsPath = TEXT("/EgoitzPineiroTools/City/Materials");
const TCHAR* FCityContentBuilder::MeshesPath = TEXT("/EgoitzPineiroTools/City/Meshes");
const TCHAR* FCityContentBuilder::SubgraphsPath = TEXT("/EgoitzPineiroTools/City/Subgraphs");

namespace
{
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
	};

	FString ObjectPathOf(const FString& PackageName)
	{
		return PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
	}

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
		FGraphWriter(UPCGGraph* InGraph, TFunction<void(const FString&)> InOnError)
			: Graph(InGraph)
			, OnError(MoveTemp(InOnError))
		{
		}

		UPCGNode* Add(UClass* SettingsClass, int32 X, int32 Y)
		{
			if (!SettingsClass)
			{
				return nullptr;
			}
			UPCGSettings* Settings = nullptr;
			UPCGNode* Node = Graph->AddNodeOfType(SettingsClass, Settings);
			if (!Node)
			{
				Fail(FString::Printf(TEXT("no se ha podido crear un nodo %s."), *SettingsClass->GetName()));
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
				Fail(FString::Printf(TEXT("no existe el nodo PCG %s en esta versión."), ClassName));
				return nullptr;
			}
			return Add(Class, X, Y);
		}

		void SetPosition(UPCGNode* Node, int32 X, int32 Y)
		{
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

		/** Copies a typed struct (an attribute selector, for example) into a property of the node settings. */
		template <typename T>
		void SetStruct(UPCGNode* Node, const TCHAR* PropertyName, const T& Value)
		{
			UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
			if (!Settings)
			{
				return;
			}
			FStructProperty* Property = FindFProperty<FStructProperty>(Settings->GetClass(), PropertyName);
			if (!Property || Property->Struct != T::StaticStruct())
			{
				Fail(FString::Printf(TEXT("%s no tiene '%s' de tipo %s. Tiene: %s"), *Settings->GetClass()->GetName(), PropertyName, *T::StaticStruct()->GetName(), *ListProperties(Settings->GetClass())));
				return;
			}
			Property->CopySingleValue(Property->ContainerPtrToValuePtr<void>(Settings), &Value);
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

		/** Adds a Graph Parameter of type Double with its default value. */
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
				Fail(FString::Printf(TEXT("el nodo de parámetro para %s no tiene salida."), *ToPin.ToString()));
				return;
			}
			Connect(GetNode, GetNode->GetOutputPins()[0]->Properties.Label, To, ToPin);
		}

		/** Adds a Point pin to the graph's Input or Output node (Custom Pins in Details). */
		void AddCustomPin(UPCGNode* Node, FName Label)
		{
			UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
			if (!Settings)
			{
				return;
			}
			FArrayProperty* Array = FindFProperty<FArrayProperty>(Settings->GetClass(), TEXT("CustomPins"));
			FStructProperty* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
			if (!Inner || Inner->Struct != FPCGPinProperties::StaticStruct())
			{
				Fail(FString::Printf(TEXT("%s no tiene CustomPins. Tiene: %s"), *Settings->GetClass()->GetName(), *ListProperties(Settings->GetClass())));
				return;
			}

			const FPCGPinProperties Pin(Label, EPCGDataType::Point);
			FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Settings));
			const int32 Index = Helper.AddValue();
			Inner->CopySingleValue(Helper.GetRawPtr(Index), &Pin);

			FPropertyChangedEvent Event(Array, EPropertyChangeType::ArrayAdd);
			static_cast<UObject*>(Settings)->PostEditChangeProperty(Event);

			// The Input and Output nodes come with the graph and do not always listen to their settings,
			// so the change above may leave their pins as they were: rebuild them from the settings.
			if (!HasPin(Node, Label))
			{
				Node->SetSettingsInterface(Node->GetSettingsInterface(), /*bUpdatePins=*/true);
			}
			if (!HasPin(Node, Label))
			{
				Fail(FString::Printf(TEXT("el pin '%s' no aparece en %s después de añadirlo. Entradas: %s. Salidas: %s"),
					*Label.ToString(), *Settings->GetClass()->GetName(), *ListPins(Node->GetInputPins()), *ListPins(Node->GetOutputPins())));
			}
		}

		static bool HasPin(UPCGNode* Node, FName Label)
		{
			return Node->GetInputPin(Label) || Node->GetOutputPin(Label);
		}

		/** Makes a Subgraph node run the given graph. */
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

		UPCGGraph* GetGraph() const { return Graph; }

	private:
		void Fail(const FString& Message)
		{
			OnError(FString::Printf(TEXT("%s: %s"), *Graph->GetName(), *Message));
		}

		UPCGGraph* Graph;
		TFunction<void(const FString&)> OnError;
	};

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

FText FCityContentBuilder::FReport::ToText() const
{
	FString Text = FString::Printf(TEXT("Creados: %d · Ya existían: %d · Errores: %d"), Created.Num(), Skipped.Num(), Errors.Num());
	for (const FString& Message : Errors)
	{
		Text += TEXT("\n- ") + Message;
	}
	if (!Errors.IsEmpty())
	{
		Text += TEXT("\nLos assets que ya existen no se rehacen: para rehacer uno con errores, bórralo en el Content Browser y pulsa Crear contenido otra vez.");
	}
	return FText::FromString(Text);
}

FCityContentBuilder::FReport FCityContentBuilder::Build()
{
	FReport Report;
	FCityContentBuilder Builder(Report);
	Builder.BuildMaterials();
	Builder.BuildMeshes();
	Builder.BuildGraphs();
	Builder.SaveAll();
	return Report;
}

void FCityContentBuilder::Error(const FString& Message)
{
	UE_LOG(LogCityContent, Warning, TEXT("%s"), *Message);
	Report.Errors.Add(Message);
}

template <typename T>
T* FCityContentBuilder::FindExisting(const FString& PackageName) const
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
T* FCityContentBuilder::CreateAsset(const FString& PackageName)
{
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	T* Asset = NewObject<T>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();

	PackagesToSave.Add(Package);
	Report.Created.Add(PackageName);
	return Asset;
}

void FCityContentBuilder::BuildMaterials()
{
	// Base material: one colour parameter and one roughness parameter, nothing else
	const FString BasePackage = FString(MaterialsPath) / TEXT("M_CityColor");
	UMaterial* BaseMaterial = FindExisting<UMaterial>(BasePackage);
	if (BaseMaterial)
	{
		Report.Skipped.Add(BasePackage);
	}
	else
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
		const FString PackageName = FString(MaterialsPath) / Definition.Name;
		if (FindExisting<UMaterialInstanceConstant>(PackageName))
		{
			Report.Skipped.Add(PackageName);
			continue;
		}

		UMaterialInstanceConstant* Instance = CreateAsset<UMaterialInstanceConstant>(PackageName);
		UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, BaseMaterial);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("Color"), Definition.Color);
		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	}
}

void FCityContentBuilder::BuildMeshes()
{
	struct FSlot
	{
		FName Name;
		const TCHAR* Material;
	};

	auto BuildMesh = [this](const TCHAR* AssetName, TArrayView<const FSlot> Slots, TFunctionRef<void(FSimpleMeshBuilder&, TArrayView<const FPolygonGroupID>)> AddSolids)
	{
		const FString PackageName = FString(MeshesPath) / AssetName;
		if (FindExisting<UStaticMesh>(PackageName))
		{
			Report.Skipped.Add(PackageName);
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
			UMaterialInterface* Material = FindExisting<UMaterialInterface>(FString(MaterialsPath) / Slot.Material);
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

	// Tree: a 2 m trunk and a round crown, about 4.7 m tall
	const FSlot TreeSlots[] = { { TEXT("Trunk"), TEXT("MI_TreeTrunk") }, { TEXT("Leaves"), TEXT("MI_TreeLeaves") } };
	BuildMesh(TEXT("SM_Tree"), TreeSlots, [](FSimpleMeshBuilder& Mesh, TArrayView<const FPolygonGroupID> Groups)
	{
		Mesh.AddCylinder(Groups[0], FVector::ZeroVector, 12.0, 210.0, 8);
		Mesh.AddEllipsoid(Groups[1], FVector(0.0, 0.0, 330.0), FVector(130.0, 130.0, 140.0), 8, 12);
	});
}

void FCityContentBuilder::BuildGraphs()
{
	using namespace CityPCGNames;

	const FName In = PCGPinConstants::DefaultInputLabel;
	const FName Out = PCGPinConstants::DefaultOutputLabel;
	const FName InsideFilter(TEXT("InsideFilter"));
	const FString Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const TFunction<void(const FString&)> OnError = [this](const FString& Message) { Error(Message); };

	auto MaterialPath = [](const TCHAR* Name) { return ObjectPathOf(FString(FCityContentBuilder::MaterialsPath) / Name); };
	auto MeshPath = [](const TCHAR* Name) { return ObjectPathOf(FString(FCityContentBuilder::MeshesPath) / Name); };

	// True if the graph is new and has to be filled; false if it already existed (it is kept as it is)
	auto StartGraph = [this](const FString& PackageName, UPCGGraph*& OutGraph)
	{
		OutGraph = FindExisting<UPCGGraph>(PackageName);
		if (OutGraph)
		{
			Report.Skipped.Add(PackageName);
			return false;
		}
		OutGraph = CreateAsset<UPCGGraph>(PackageName);
		return true;
	};

	// Attribute Filter that keeps the points whose attribute equals a constant
	auto AddEqualsFilter = [](FGraphWriter& Writer, int32 X, int32 Y, FName Attribute, const TCHAR* ConstantText)
	{
		UPCGNode* Filter = Writer.AddStandard(TEXT("PCGAttributeFilteringSettings"), X, Y);
		FPCGAttributePropertyInputSelector Target;
		Target.SetAttributeName(Attribute);
		Writer.SetStruct(Filter, TEXT("TargetAttribute"), Target);
		Writer.Set(Filter, TEXT("Operator"), TEXT("Equal"));
		Writer.Set(Filter, TEXT("bUseConstantThreshold"), TEXT("True"));
		Writer.Set(Filter, TEXT("AttributeTypes"), ConstantText);
		Writer.Refresh(Filter);
		return Filter;
	};

	auto AddSpawner = [](FGraphWriter& Writer, int32 X, int32 Y, const FString& EntriesText)
	{
		UPCGNode* Spawner = Writer.AddStandard(TEXT("PCGStaticMeshSpawnerSettings"), X, Y);
		Writer.Set(Spawner, TEXT("MeshSelectorParameters.MeshEntries"), EntriesText);
		Writer.Refresh(Spawner);
		return Spawner;
	};

	auto PlaceInputOutput = [](FGraphWriter& Writer, int32 InputX, int32 OutputX)
	{
		Writer.SetPosition(Writer.GetGraph()->GetInputNode(), InputX, 0);
		Writer.SetPosition(Writer.GetGraph()->GetOutputNode(), OutputX, 0);
	};

	// --- SG_Streets: crossings and street segments -------------------------------------------
	UPCGGraph* Streets = nullptr;
	if (StartGraph(FString(SubgraphsPath) / TEXT("SG_Streets"), Streets))
	{
		FGraphWriter Writer(Streets, OnError);
		Writer.AddParameter(TEXT("CitySize"), 400.0);
		Writer.AddParameter(TEXT("BlockSize"), 60.0);
		PlaceInputOutput(Writer, -1000, 900);

		UPCGNode* GetCitySize = Writer.AddGetParameter(TEXT("CitySize"), -700, -100);
		UPCGNode* GetBlockSize = Writer.AddGetParameter(TEXT("BlockSize"), -700, 100);
		UPCGNode* Crossings = Writer.Add(UPCGCityCrossingsSettings::StaticClass(), -350, 0);
		UPCGNode* Segments = Writer.Add(UPCGCityStreetSegmentsSettings::StaticClass(), 0, 200);
		UPCGNode* Merge = Writer.AddStandard(TEXT("PCGMergeSettings"), 300, 0);
		UPCGNode* Spawner = AddSpawner(Writer, 600, 0, MeshEntriesText(Cube, { MaterialPath(TEXT("MI_Road")) }));
		UPCGNode* Output = Streets->GetOutputNode();
		Writer.AddCustomPin(Output, CrossingsPin);

		Writer.ConnectParameter(GetCitySize, Crossings, TEXT("CitySize"));
		Writer.ConnectParameter(GetBlockSize, Crossings, TEXT("BlockSize"));
		Writer.Connect(Crossings, CrossingsPin, Segments, CrossingsPin);
		Writer.Connect(Crossings, CrossingsPin, Merge, In);
		Writer.Connect(Segments, SegmentsPin, Merge, In);
		Writer.Connect(Merge, Out, Spawner, In);
		Writer.Connect(Crossings, CrossingsPin, Output, CrossingsPin);
	}

	// --- SG_Blocks: blocks (sidewalk level) and the sidewalk slots for street furniture -------
	const FName SidewalkSlotsPin(TEXT("SidewalkSlots"));
	UPCGGraph* Blocks = nullptr;
	if (StartGraph(FString(SubgraphsPath) / TEXT("SG_Blocks"), Blocks))
	{
		FGraphWriter Writer(Blocks, OnError);
		PlaceInputOutput(Writer, -700, 500);
		UPCGNode* Input = Blocks->GetInputNode();
		UPCGNode* Output = Blocks->GetOutputNode();
		Writer.AddCustomPin(Input, CrossingsPin);
		Writer.AddCustomPin(Output, BlocksPin);
		Writer.AddCustomPin(Output, SidewalkSlotsPin);

		UPCGNode* CityBlocks = Writer.Add(UPCGCityBlocksSettings::StaticClass(), -350, 0);
		UPCGNode* Spawner = AddSpawner(Writer, 50, -200, MeshEntriesText(Cube, { MaterialPath(TEXT("MI_Sidewalk")) }));
		UPCGNode* Slots = Writer.Add(UPCGCitySidewalkSlotsSettings::StaticClass(), 50, 200);

		Writer.Connect(Input, CrossingsPin, CityBlocks, CrossingsPin);
		Writer.Connect(CityBlocks, BlocksPin, Spawner, In);
		Writer.Connect(CityBlocks, BlocksPin, Slots, BlocksPin);
		Writer.Connect(Input, CrossingsPin, Slots, CrossingsPin);
		Writer.Connect(CityBlocks, BlocksPin, Output, BlocksPin);
		Writer.Connect(Slots, SlotsPin, Output, SidewalkSlotsPin);
	}

	// --- SG_Buildings: lots, density and buildings ----------------------------------------------
	UPCGGraph* Buildings = nullptr;
	if (StartGraph(FString(SubgraphsPath) / TEXT("SG_Buildings"), Buildings))
	{
		FGraphWriter Writer(Buildings, OnError);
		Writer.AddParameter(TEXT("BuildingDensity"), 0.6);
		Writer.AddParameter(TEXT("BuildingHeight"), 30.0);
		Writer.AddParameter(TEXT("BuildingVariation"), 0.5);
		PlaceInputOutput(Writer, -1300, 1700);
		UPCGNode* Input = Buildings->GetInputNode();
		UPCGNode* Output = Buildings->GetOutputNode();
		Writer.AddCustomPin(Input, BlocksPin);
		Writer.AddCustomPin(Output, BuildingsPin);

		UPCGNode* Lots = Writer.Add(UPCGCityLotsSettings::StaticClass(), -1000, 0);
		UPCGNode* FrontageFilter = AddEqualsFilter(Writer, -700, 0, CityPCGNames::Frontage, TEXT("(Type=Boolean,BoolValue=True)"));

		UPCGNode* Noise = Writer.AddStandard(TEXT("PCGAttributeNoiseSettings"), -400, 0);
		FPCGAttributePropertyInputSelector DensitySource;
		DensitySource.SetPointProperty(EPCGPointProperties::Density);
		Writer.SetStruct(Noise, TEXT("InputSource"), DensitySource);
		Writer.Set(Noise, TEXT("Mode"), TEXT("Set"));
		Writer.Set(Noise, TEXT("NoiseMin"), TEXT("0.0"));
		Writer.Set(Noise, TEXT("NoiseMax"), TEXT("1.0"));
		Writer.Refresh(Noise);

		UPCGNode* GetDensity = Writer.AddGetParameter(TEXT("BuildingDensity"), -400, 250);
		UPCGNode* DensityFilter = Writer.AddStandard(TEXT("PCGDensityFilterSettings"), -100, 0);
		Writer.Set(DensityFilter, TEXT("LowerBound"), TEXT("0.0"));
		Writer.Refresh(DensityFilter);

		UPCGNode* GetHeight = Writer.AddGetParameter(TEXT("BuildingHeight"), -100, 250);
		UPCGNode* GetVariation = Writer.AddGetParameter(TEXT("BuildingVariation"), -100, 400);
		UPCGNode* CityBuildings = Writer.Add(UPCGCityBuildingsSettings::StaticClass(), 250, 0);

		UPCGNode* Jitter = Writer.AddStandard(TEXT("PCGTransformPointsSettings"), 600, 0);
		Writer.Set(Jitter, TEXT("OffsetMin"), TEXT("(X=-50.0,Y=-50.0,Z=0.0)"));
		Writer.Set(Jitter, TEXT("OffsetMax"), TEXT("(X=50.0,Y=50.0,Z=0.0)"));
		Writer.Refresh(Jitter);

		UPCGNode* Pruning = Writer.AddStandard(TEXT("PCGSelfPruningSettings"), 950, 0);
		UPCGNode* Spawner = AddSpawner(Writer, 1300, 0, MeshEntriesText(Cube,
			{ MaterialPath(TEXT("MI_Building_A")), MaterialPath(TEXT("MI_Building_B")), MaterialPath(TEXT("MI_Building_C")) }));

		Writer.Connect(Input, BlocksPin, Lots, BlocksPin);
		Writer.Connect(Lots, LotsPin, FrontageFilter, In);
		Writer.Connect(FrontageFilter, InsideFilter, Noise, In);
		Writer.Connect(Noise, Out, DensityFilter, In);
		Writer.ConnectParameter(GetDensity, DensityFilter, TEXT("UpperBound"));
		Writer.Connect(DensityFilter, Out, CityBuildings, LotsPin);
		Writer.ConnectParameter(GetHeight, CityBuildings, TEXT("BuildingHeight"));
		Writer.ConnectParameter(GetVariation, CityBuildings, TEXT("BuildingVariation"));
		Writer.Connect(CityBuildings, BuildingsPin, Jitter, In);
		Writer.Connect(Jitter, Out, Pruning, In);
		Writer.Connect(Pruning, Out, Spawner, In);
		Writer.Connect(Pruning, Out, Output, BuildingsPin);
	}

	// --- SG_StreetLamps and SG_Trees: one subgraph per kind of street furniture -----------------
	UPCGGraph* StreetLamps = nullptr;
	if (StartGraph(FString(SubgraphsPath) / TEXT("SG_StreetLamps"), StreetLamps))
	{
		FGraphWriter Writer(StreetLamps, OnError);
		PlaceInputOutput(Writer, -700, 500);
		UPCGNode* Input = StreetLamps->GetInputNode();
		Writer.AddCustomPin(Input, SlotsPin);

		UPCGNode* Filter = AddEqualsFilter(Writer, -350, 0, PropType, TEXT("(Type=Integer64,IntValue=0)"));
		UPCGNode* Spawner = AddSpawner(Writer, 50, 0, MeshEntriesText(MeshPath(TEXT("SM_StreetLamp")), {}));

		Writer.Connect(Input, SlotsPin, Filter, In);
		Writer.Connect(Filter, InsideFilter, Spawner, In);
	}

	UPCGGraph* Trees = nullptr;
	if (StartGraph(FString(SubgraphsPath) / TEXT("SG_Trees"), Trees))
	{
		FGraphWriter Writer(Trees, OnError);
		PlaceInputOutput(Writer, -700, 800);
		UPCGNode* Input = Trees->GetInputNode();
		Writer.AddCustomPin(Input, SlotsPin);

		UPCGNode* Filter = AddEqualsFilter(Writer, -350, 0, PropType, TEXT("(Type=Integer64,IntValue=1)"));
		UPCGNode* Variation = Writer.AddStandard(TEXT("PCGTransformPointsSettings"), 0, 0);
		Writer.Set(Variation, TEXT("RotationMax"), TEXT("(Pitch=0.0,Yaw=360.0,Roll=0.0)"));
		Writer.Set(Variation, TEXT("ScaleMin"), TEXT("(X=0.8,Y=0.8,Z=0.8)"));
		Writer.Set(Variation, TEXT("ScaleMax"), TEXT("(X=1.3,Y=1.3,Z=1.3)"));
		Writer.Set(Variation, TEXT("bUniformScale"), TEXT("True"));
		Writer.Refresh(Variation);
		UPCGNode* Spawner = AddSpawner(Writer, 400, 0, MeshEntriesText(MeshPath(TEXT("SM_Tree")), {}));

		Writer.Connect(Input, SlotsPin, Filter, In);
		Writer.Connect(Filter, InsideFilter, Variation, In);
		Writer.Connect(Variation, Out, Spawner, In);
	}

	// --- PCG_City: only wires the systems and passes the parameters down ------------------------
	UPCGGraph* City = nullptr;
	if (StartGraph(FString(RootPath) / TEXT("PCG_City"), City))
	{
		FGraphWriter Writer(City, OnError);
		Writer.AddParameter(TEXT("CitySize"), 400.0);
		Writer.AddParameter(TEXT("BlockSize"), 60.0);
		Writer.AddParameter(TEXT("BuildingDensity"), 0.6);
		Writer.AddParameter(TEXT("BuildingHeight"), 30.0);
		Writer.AddParameter(TEXT("BuildingVariation"), 0.5);
		PlaceInputOutput(Writer, -1100, 1100);

		auto AddSubgraph = [&Writer](UPCGGraph* Subgraph, int32 X, int32 Y)
		{
			UPCGNode* Node = Writer.AddStandard(TEXT("PCGSubgraphSettings"), X, Y);
			Writer.SetSubgraph(Node, Subgraph);
			return Node;
		};

		UPCGNode* GetCitySize = Writer.AddGetParameter(TEXT("CitySize"), -800, -100);
		UPCGNode* GetBlockSize = Writer.AddGetParameter(TEXT("BlockSize"), -800, 50);
		UPCGNode* StreetsNode = AddSubgraph(Streets, -450, 0);
		UPCGNode* BlocksNode = AddSubgraph(Blocks, -50, 0);
		UPCGNode* GetDensity = Writer.AddGetParameter(TEXT("BuildingDensity"), 0, -350);
		UPCGNode* GetHeight = Writer.AddGetParameter(TEXT("BuildingHeight"), 0, -250);
		UPCGNode* GetVariation = Writer.AddGetParameter(TEXT("BuildingVariation"), 0, -150);
		UPCGNode* BuildingsNode = AddSubgraph(Buildings, 400, -200);
		UPCGNode* LampsNode = AddSubgraph(StreetLamps, 400, 100);
		UPCGNode* TreesNode = AddSubgraph(Trees, 400, 300);

		Writer.ConnectParameter(GetCitySize, StreetsNode, TEXT("CitySize"));
		Writer.ConnectParameter(GetBlockSize, StreetsNode, TEXT("BlockSize"));
		Writer.Connect(StreetsNode, CrossingsPin, BlocksNode, CrossingsPin);
		Writer.Connect(BlocksNode, BlocksPin, BuildingsNode, BlocksPin);
		Writer.ConnectParameter(GetDensity, BuildingsNode, TEXT("BuildingDensity"));
		Writer.ConnectParameter(GetHeight, BuildingsNode, TEXT("BuildingHeight"));
		Writer.ConnectParameter(GetVariation, BuildingsNode, TEXT("BuildingVariation"));
		Writer.Connect(BlocksNode, SidewalkSlotsPin, LampsNode, SlotsPin);
		Writer.Connect(BlocksNode, SidewalkSlotsPin, TreesNode, SlotsPin);
	}
}

void FCityContentBuilder::SaveAll()
{
	if (PackagesToSave.IsEmpty())
	{
		return;
	}
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty=*/false))
	{
		Error(TEXT("No se han podido guardar todos los assets nuevos. Usa File > Save All."));
	}
}

#undef LOCTEXT_NAMESPACE
