#include "CityContentBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityContent, Log, All);

#define LOCTEXT_NAMESPACE "CityContentBuilder"

const TCHAR* FCityContentBuilder::RootPath = TEXT("/EgoitzPineiroTools/City");
const TCHAR* FCityContentBuilder::MaterialsPath = TEXT("/EgoitzPineiroTools/City/Materials");
const TCHAR* FCityContentBuilder::MeshesPath = TEXT("/EgoitzPineiroTools/City/Meshes");

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
}

FText FCityContentBuilder::FReport::ToText() const
{
	FString Text = FString::Printf(TEXT("Creados: %d · Ya existían: %d · Errores: %d"), Created.Num(), Skipped.Num(), Errors.Num());
	for (const FString& Message : Errors)
	{
		Text += TEXT("\n- ") + Message;
	}
	return FText::FromString(Text);
}

FCityContentBuilder::FReport FCityContentBuilder::Build()
{
	FReport Report;
	FCityContentBuilder Builder(Report);
	Builder.BuildMaterials();
	Builder.BuildMeshes();
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
