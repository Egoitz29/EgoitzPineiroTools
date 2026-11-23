#include "SCityGeneratorPanel.h"

#include "CityGeneratorTool.h"
#include "HAL/PlatformTime.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCityGeneratorPanel"

template <typename T>
TSharedRef<SWidget> SCityGeneratorPanel::MakeParameterRow(const FText& Label, const FText& Tooltip, T& Value, T MinValue, T MaxValue, T Delta)
{
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center).Padding(0.0f, 2.0f, 8.0f, 2.0f)
		[
			SNew(STextBlock).Text(Label)
		]
		+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center).Padding(0.0f, 2.0f)
		[
			SNew(SSpinBox<T>)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(MinValue)
			.MaxSliderValue(MaxValue)
			.Delta(Delta)
			.Value_Lambda([&Value]() { return Value; })
			.OnValueChanged_Lambda([&Value](T NewValue) { Value = NewValue; })
			.OnValueCommitted_Lambda([this, &Value](T NewValue, ETextCommit::Type) { Value = NewValue; OnParameterCommitted(); })
			.OnEndSliderMovement_Lambda([this](T) { OnParameterCommitted(); })
		];
}

void SCityGeneratorPanel::Construct(const FArguments& InArgs)
{
	Tool = InArgs._Tool;
	check(Tool.IsValid());
	FCityGenerationParams& Params = Tool->Params;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(12.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Generador procedural de ciudades"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Intro", "Ajusta los parámetros y pulsa Generar. Misma seed y mismos parámetros, misma ciudad."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(this, &SCityGeneratorPanel::GetGraphStatusText)
				.AutoWrapText(true)
			]

			// City layout
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("CitySection", "Ciudad")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("CitySize", "Tamaño de la ciudad (m)"),
					LOCTEXT("CitySizeTip", "Lado de la ciudad cuadrada. Cuanto mayor, más manzanas."),
					Params.CitySize, 50.0, 3000.0, 10.0)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("BlockSize", "Tamaño de manzana (m)"),
					LOCTEXT("BlockSizeTip", "Lado de cada manzana. Manzanas pequeñas = más cruces."),
					Params.BlockSize, 15.0, 300.0, 1.0)
			]

			// Buildings
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("BuildingsSection", "Edificios")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("Density", "Densidad (0-1)"),
					LOCTEXT("DensityTip", "Parte de las parcelas de fachada que se construyen: 0 = zona abierta, 1 = cañón urbano."),
					Params.BuildingDensity, 0.0, 1.0, 0.05)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("Height", "Altura media (m)"),
					LOCTEXT("HeightTip", "Altura media de los edificios."),
					Params.BuildingHeight, 3.0, 300.0, 1.0)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("Variation", "Variación (0-1)"),
					LOCTEXT("VariationTip", "0 = todos los edificios iguales; 1 = alturas y huellas muy distintas."),
					Params.BuildingVariation, 0.0, 1.0, 0.05)
			]

			// Seed
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("SeedSection", "Semilla")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					MakeParameterRow(LOCTEXT("Seed", "Seed"),
						LOCTEXT("SeedTip", "Identifica el escenario: la misma seed siempre da la misma ciudad."),
						Params.Seed, 0, 9999999, 1)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RandomSeed", "Aleatoria"))
					.ToolTipText(LOCTEXT("RandomSeedTip", "Elige otra seed al azar."))
					.OnClicked(this, &SCityGeneratorPanel::OnRandomSeedClicked)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bAutoRegenerate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bAutoRegenerate = State == ECheckBoxState::Checked; })
				.ToolTipText(LOCTEXT("AutoTip", "Si ya hay una ciudad, se regenera al soltar un deslizador o escribir un valor."))
				[
					SNew(STextBlock).Text(LOCTEXT("Auto", "Regenerar al cambiar un valor"))
				]
			]

			// Actions
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Generate", "Generar ciudad"))
					.ToolTipText(LOCTEXT("GenerateTip", "Borra la ciudad anterior y genera una nueva con estos parámetros."))
					.OnClicked(this, &SCityGeneratorPanel::OnGenerateClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Cleanup", "Limpiar ciudad"))
					.ToolTipText(LOCTEXT("CleanupTip", "Elimina por completo la ciudad generada."))
					.OnClicked(this, &SCityGeneratorPanel::OnCleanupClicked)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusText; })
				.AutoWrapText(true)
			]

			// What is in the level right now
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("StatsSection", "Ciudad en el nivel")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &SCityGeneratorPanel::GetStatsText)
				.AutoWrapText(true)
			]
		]
	];
}

void SCityGeneratorPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (InCurrentTime >= NextStatsRefreshTime)
	{
		CachedStats = Tool->ComputeStats();
		NextStatsRefreshTime = InCurrentTime + 0.5;
	}
}

TSharedRef<SWidget> SCityGeneratorPanel::MakeSectionHeader(const FText& Title) const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SSeparator)
		];
}

FReply SCityGeneratorPanel::OnGenerateClicked()
{
	Tool->Generate(/*bFullRebuild=*/true, StatusText);
	NextStatsRefreshTime = 0.0;
	return FReply::Handled();
}

FReply SCityGeneratorPanel::OnRandomSeedClicked()
{
	Tool->Params.Seed = FMath::RandRange(0, 9999999); // only picks the number: the city still comes from it
	OnParameterCommitted();
	return FReply::Handled();
}

void SCityGeneratorPanel::OnParameterCommitted()
{
	if (bAutoRegenerate && Tool->HasCity())
	{
		Tool->Generate(/*bFullRebuild=*/false, StatusText);
		NextStatsRefreshTime = 0.0;
	}
}

FReply SCityGeneratorPanel::OnCleanupClicked()
{
	const int32 Removed = Tool->Cleanup();
	StatusText = Removed > 0
		? FText::Format(LOCTEXT("Cleaned", "Ciudad eliminada ({0} actor/es borrados)."), Removed)
		: LOCTEXT("NothingToClean", "No había ninguna ciudad que limpiar.");
	NextStatsRefreshTime = 0.0;
	return FReply::Handled();
}

FText SCityGeneratorPanel::GetStatsText() const
{
	const FCityStats& Stats = CachedStats;
	if (Stats.NumCities == 0)
	{
		return LOCTEXT("NoCity", "No hay ninguna ciudad generada.");
	}

	TArray<FString> Lines;
	if (Stats.bGenerating)
	{
		Lines.Add(LOCTEXT("Generating", "Generando...").ToString());
	}
	if (Stats.NumCities > 1)
	{
		Lines.Add(FText::Format(LOCTEXT("SeveralCities", "Atención: hay {0} ciudades en el nivel. Generar las sustituye por una."), Stats.NumCities).ToString());
	}
	Lines.Add(FText::Format(LOCTEXT("Total", "Instancias: {0}"), Stats.TotalInstances).ToString());
	for (const TPair<FString, int32>& Pair : Stats.InstancesPerType)
	{
		Lines.Add(FString::Printf(TEXT("    %s: %d"), *Pair.Key, Pair.Value));
	}
	Lines.Add(FText::Format(LOCTEXT("Fingerprint", "Huella: {0}  (misma seed y parámetros = misma huella)"), FText::FromString(FString::Printf(TEXT("%08X"), Stats.Fingerprint))).ToString());

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText SCityGeneratorPanel::GetGraphStatusText() const
{
	// The text is read every frame but the check touches the disk: refresh it once per second
	const double Now = FPlatformTime::Seconds();
	if (Now >= NextGraphCheckTime)
	{
		bGraphAvailable = FCityGeneratorTool::IsGraphAvailable();
		NextGraphCheckTime = Now + 1.0;
	}

	return bGraphAvailable
		? LOCTEXT("GraphFound", "Grafo PCG_City: encontrado.")
		: FText::Format(LOCTEXT("GraphMissing", "Grafo PCG_City: NO encontrado en {0}. Créalo siguiendo Docs/Grafos.md."), FText::FromString(FCityGeneratorTool::GraphPackage));
}

#undef LOCTEXT_NAMESPACE
