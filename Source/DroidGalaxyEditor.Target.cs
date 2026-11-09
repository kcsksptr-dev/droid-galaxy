using UnrealBuildTool;

public class DroidGalaxyEditorTarget : TargetRules
{
	public DroidGalaxyEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
		ExtraModuleNames.Add("DroidGalaxy");
	}
}
