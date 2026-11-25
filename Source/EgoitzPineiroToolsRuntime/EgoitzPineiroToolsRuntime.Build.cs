using UnrealBuildTool;

// Runtime module: the city's PCG nodes and the actor that holds a city.
// Runtime (not Editor) so the nodes also exist in a packaged build.
public class EgoitzPineiroToolsRuntime : ModuleRules
{
	public EgoitzPineiroToolsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PCG"
		});
	}
}
