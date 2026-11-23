#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"

class FSlateStyleSet;
class SDockTab;

/**
 * Editor module of the plugin. Registers the "Generador de ciudades" tab and opens it from
 * Tools > Generador de ciudades and from a button in the level editor toolbar.
 */
class FEgoitzPineiroToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterStyle();
	void RegisterMenus();
	void OpenCityGeneratorTab();
	TSharedRef<SDockTab> OnSpawnCityGeneratorTab(const FSpawnTabArgs& Args);

	/** Icon of the tab, the menu entry and the toolbar button (Resources/Icon128.png). */
	TSharedPtr<FSlateStyleSet> StyleSet;
};
