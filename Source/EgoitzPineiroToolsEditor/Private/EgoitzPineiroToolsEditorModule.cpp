#include "EgoitzPineiroToolsEditorModule.h"

#include "Brushes/SlateImageBrush.h"
#include "CityGenerator.h"
#include "Interfaces/IPluginManager.h"
#include "SCityPanel.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FEgoitzPineiroToolsEditorModule"

namespace
{
	const FName CityTabName(TEXT("EPT_CityGenerator"));
	const FName StyleSetName(TEXT("EgoitzPineiroToolsStyle"));
	const FName CityIconName(TEXT("EgoitzPineiroTools.CityIcon"));
}

void FEgoitzPineiroToolsEditorModule::StartupModule()
{
	Generator = MakeShared<FCityGenerator>();
	RegisterStyle();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CityTabName,
		FOnSpawnTab::CreateRaw(this, &FEgoitzPineiroToolsEditorModule::OnSpawnCityTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Generador de ciudades"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Configura, genera y limpia ciudades procedurales con PCG."))
		.SetIcon(FSlateIcon(StyleSetName, CityIconName));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEgoitzPineiroToolsEditorModule::RegisterMenus));
}

void FEgoitzPineiroToolsEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CityTabName);

	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}

	Generator.Reset();
}

void FEgoitzPineiroToolsEditorModule::RegisterStyle()
{
	StyleSet = MakeShared<FSlateStyleSet>(StyleSetName);

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EgoitzPineiroTools"));
	const FString IconPath = Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Resources/Icon128.png") : FString();

	StyleSet->Set(CityIconName, new FSlateImageBrush(IconPath, FVector2D(40.0, 40.0)));
	StyleSet->Set(FName(TEXT("EgoitzPineiroTools.CityIcon.Small")), new FSlateImageBrush(IconPath, FVector2D(20.0, 20.0)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FEgoitzPineiroToolsEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	const FSlateIcon Icon(StyleSetName, CityIconName);
	const FUIAction OpenAction(FExecuteAction::CreateRaw(this, &FEgoitzPineiroToolsEditorModule::OpenCityTab));

	// Tools > Generador de ciudades
	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("EgoitzPineiroTools");
		Section.AddMenuEntry("EPT_OpenCityGenerator",
			LOCTEXT("MenuLabel", "Generador de ciudades"),
			LOCTEXT("MenuTooltip", "Abre el panel del generador procedural de ciudades."),
			Icon, OpenAction);
	}

	// "Ciudad" button in the level editor toolbar, next to Play
	if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
	{
		FToolMenuSection& Section = Toolbar->FindOrAddSection("PluginTools");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton("EPT_CityGeneratorButton", OpenAction,
			LOCTEXT("ToolbarLabel", "Ciudad"),
			LOCTEXT("ToolbarTooltip", "Abre el generador procedural de ciudades."),
			Icon));
	}
}

void FEgoitzPineiroToolsEditorModule::OpenCityTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(CityTabName);
}

TSharedRef<SDockTab> FEgoitzPineiroToolsEditorModule::OnSpawnCityTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SCityPanel)
			.Generator(Generator)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEgoitzPineiroToolsEditorModule, EgoitzPineiroToolsEditor)
