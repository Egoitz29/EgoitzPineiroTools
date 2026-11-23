using UnrealBuildTool;

// Runtime module: the custom PCG nodes and the city actor.
// Runtime (not Editor) because a PCG node must also exist in a packaged game.
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
