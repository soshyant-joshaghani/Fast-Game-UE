using UnrealBuildTool;
using System.Collections.Generic;

public class FastGameUETarget : TargetRules
{
	public FastGameUETarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("FastGameUE");
	}
}
