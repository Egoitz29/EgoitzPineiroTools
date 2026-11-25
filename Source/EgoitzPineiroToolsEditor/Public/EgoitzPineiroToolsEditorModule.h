#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"

class FCityGenerator;
class FSlateStyleSet;
class SDockTab;

/**
 * Editor module of the plugin. Registers the "Generador de ciudades" tab and opens it from
 * Tools > Generador de ciudades and from the "Ciudad" button of the level editor toolbar.
 */
class FEgoitzPineiroToolsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterStyle();
	void RegisterMenus();
	void OpenCityTab();
	TSharedRef<SDockTab> OnSpawnCityTab(const FSpawnTabArgs& Args);

	/** Parameters and actions of the tool. Lives as long as the module, so the values survive closing the tab. */
	TSharedPtr<FCityGenerator> Generator;

	/** Icon of the tab, the menu entry and the toolbar button (Resources/Icon128.png). */
	TSharedPtr<FSlateStyleSet> StyleSet;
};
