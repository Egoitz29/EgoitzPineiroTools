using UnrealBuildTool;

// Editor module: the "Generador de ciudades" panel, the content it creates and the toolbar/menu entries.
// It never reaches a packaged build.
public class EgoitzPineiroToolsEditor : ModuleRules
{
	public EgoitzPineiroToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"ApplicationCore",
			"AssetRegistry",
			"InputCore",
			"MaterialEditor",
			"MeshDescription",
			"StaticMeshDescription",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"Projects",
			"PCG",
			"EgoitzPineiroToolsRuntime"
		});
	}
}
