#include "SCityPanel.h"

#include "CityGrid.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCityPanel"

template <typename T>
TSharedRef<SWidget> SCityPanel::MakeParameterRow(const FText& Label, const FText& Tooltip, T& Value, T MinValue, T MaxValue, T Delta)
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

void SCityPanel::Construct(const FArguments& InArgs)
{
	Generator = InArgs._Generator;
	check(Generator.IsValid());
	FCityParams& Params = Generator->Params;

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
				.Text(LOCTEXT("Intro", "Ajusta los parámetros y pulsa Generar ciudad. La misma seed con los mismos parámetros da siempre la misma ciudad."))
				.AutoWrapText(true)
			]

			// --- Streets and blocks
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
					LOCTEXT("BlockSizeTip", "Lado de cada manzana, acera incluida. Manzanas pequeñas generan más cruces."),
					Params.BlockSize, 15.0, 300.0, 1.0)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SCityPanel::GetGridText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			// --- Buildings
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("BuildingsSection", "Edificios")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("Density", "Densidad (0-1)"),
					LOCTEXT("DensityTip", "Parte de las parcelas que se construyen: 0 = zona abierta, 1 = cañón urbano."),
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
					LOCTEXT("VariationTip", "0 = todos los edificios iguales. 1 = alturas, huellas y posiciones muy distintas."),
					Params.BuildingVariation, 0.0, 1.0, 0.05)
			]

			// --- Parks
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("ParksSection", "Parques")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("ParkRatio", "Manzanas que son parque (0-1)"),
					LOCTEXT("ParkRatioTip", "Parte de las manzanas que se convierten en parque en lugar de edificarse."),
					Params.ParkRatio, 0.0, 1.0, 0.05)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("ParkTreeDensity", "Densidad de árboles (0-1)"),
					LOCTEXT("ParkTreeDensityTip", "Parte de los huecos del parque (uno cada 6 m) que tienen un árbol o un arbusto."),
					Params.ParkTreeDensity, 0.0, 1.0, 0.05)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeParameterRow(LOCTEXT("ParkTreeScale", "Variación de escala (0-1)"),
					LOCTEXT("ParkTreeScaleTip", "0 = toda la vegetación del mismo tamaño. 1 = de la mitad a una vez y media."),
					Params.ParkTreeScaleVariation, 0.0, 1.0, 0.05)
			]

			// --- Seed
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
					.OnClicked(this, &SCityPanel::OnRandomSeedClicked)
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

			// --- Actions
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Generate", "Generar ciudad"))
					.ToolTipText(LOCTEXT("GenerateTip", "Borra la ciudad anterior y genera una nueva con estos parámetros. La primera vez crea también el contenido del plugin."))
					.OnClicked(this, &SCityPanel::OnGenerateClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Cleanup", "Limpiar ciudad"))
					.ToolTipText(LOCTEXT("CleanupTip", "Elimina por completo la ciudad generada."))
					.OnClicked(this, &SCityPanel::OnCleanupClicked)
				]
			]

			// --- Messages: selectable, and a button to copy them
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(SBox)
				.MaxDesiredHeight(220.0f)
				[
					SNew(SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AutoWrapText(true)
					.Text(this, &SCityPanel::GetMessagesText)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("Copy", "Copiar mensajes"))
				.ToolTipText(LOCTEXT("CopyTip", "Copia los mensajes al portapapeles."))
				.OnClicked(this, &SCityPanel::OnCopyMessagesClicked)
			]

			// --- What is in the level right now
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("StatsSection", "Ciudad en el nivel")) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &SCityPanel::GetStatsText)
				.AutoWrapText(true)
			]

			// --- Content of the plugin
			+ SVerticalBox::Slot().AutoHeight()[ MakeSectionHeader(LOCTEXT("ContentSection", "Contenido")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ContentInfo", "Los materiales, las mallas y los grafos se crean solos al generar. Rehacer grafos los vuelve a montar desde el código (se pierden los cambios hechos a mano en ellos)."))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(LOCTEXT("RebuildGraphs", "Rehacer grafos"))
				.ToolTipText(LOCTEXT("RebuildGraphsTip", "Vuelve a montar PCG_City y sus subgrafos desde el código y genera la ciudad otra vez."))
				.OnClicked(this, &SCityPanel::OnRebuildGraphsClicked)
			]
		]
	];
}

void SCityPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (InCurrentTime >= NextStatsRefreshTime)
	{
		CachedStats = Generator->ComputeStats();
		NextStatsRefreshTime = InCurrentTime + 0.5;
	}
}

TSharedRef<SWidget> SCityPanel::MakeSectionHeader(const FText& Title) const
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

FReply SCityPanel::OnGenerateClicked()
{
	Generator->Generate(/*bFullRebuild=*/true, StatusText);
	NextStatsRefreshTime = 0.0;
	return FReply::Handled();
}

FReply SCityPanel::OnCleanupClicked()
{
	const int32 Removed = Generator->Cleanup();
	StatusText = Removed > 0
		? FText::Format(LOCTEXT("Cleaned", "Ciudad eliminada ({0} actor/es borrados)."), Removed)
		: LOCTEXT("NothingToClean", "No había ninguna ciudad que limpiar.");
	NextStatsRefreshTime = 0.0;
	return FReply::Handled();
}

FReply SCityPanel::OnRandomSeedClicked()
{
	Generator->Params.Seed = FMath::RandRange(0, 9999999); // only picks the number: the city still comes from it
	OnParameterCommitted();
	return FReply::Handled();
}

FReply SCityPanel::OnRebuildGraphsClicked()
{
	Generator->Cleanup(); // nothing may regenerate while the graphs are rebuilt
	const FCityContent::FReport Report = FCityContent::Ensure(/*bRebuildGraphs=*/true);
	Generator->Generate(/*bFullRebuild=*/true, StatusText);
	Generator->LastContentReport = Report; // what the rebuild did, not the check after it
	NextStatsRefreshTime = 0.0;
	return FReply::Handled();
}

FReply SCityPanel::OnCopyMessagesClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*GetMessagesText().ToString());
	return FReply::Handled();
}

void SCityPanel::OnParameterCommitted()
{
	if (bAutoRegenerate && Generator->HasCity())
	{
		Generator->Generate(/*bFullRebuild=*/false, StatusText);
		NextStatsRefreshTime = 0.0;
	}
}

FText SCityPanel::GetGridText() const
{
	const FCityGrid Grid = FCityGrid::Make(Generator->Params.CitySize, Generator->Params.BlockSize);
	return FText::Format(LOCTEXT("Grid", "{0} x {0} manzanas, {1} cruces, {2} m de lado."),
		Grid.BlocksPerSide, Grid.CrossingsPerSide() * Grid.CrossingsPerSide(), FMath::RoundToInt32(Grid.Extent() / 100.0));
}

FText SCityPanel::GetMessagesText() const
{
	FString Text = StatusText.ToString();
	if (!Generator->LastContentReport.IsEmpty())
	{
		Text += (Text.IsEmpty() ? TEXT("") : TEXT("\n")) + Generator->LastContentReport.ToString();
	}
	return FText::FromString(Text);
}

FText SCityPanel::GetStatsText() const
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

#undef LOCTEXT_NAMESPACE
