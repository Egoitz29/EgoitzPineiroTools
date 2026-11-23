#include "CityContentBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityContent, Log, All);

#define LOCTEXT_NAMESPACE "CityContentBuilder"

const TCHAR* FCityContentBuilder::RootPath = TEXT("/EgoitzPineiroTools/City");
const TCHAR* FCityContentBuilder::MaterialsPath = TEXT("/EgoitzPineiroTools/City/Materials");

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
