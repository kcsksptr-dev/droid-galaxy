using UnrealBuildTool;

public class DroidGalaxy : ModuleRules
{
	public DroidGalaxy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "RenderCore" });
	}
}
