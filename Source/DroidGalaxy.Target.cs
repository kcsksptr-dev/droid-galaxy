using UnrealBuildTool;

public class DroidGalaxyTarget : TargetRules
{
	public DroidGalaxyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
		ExtraModuleNames.Add("DroidGalaxy");
	}
}
