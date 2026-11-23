using UnrealBuildTool;

// Editor module: the City Generator panel (Slate tab, Tools menu entry and toolbar button).
public class EgoitzPineiroTools : ModuleRules
{
	public EgoitzPineiroTools(ReadOnlyTargetRules Target) : base(Target)
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
