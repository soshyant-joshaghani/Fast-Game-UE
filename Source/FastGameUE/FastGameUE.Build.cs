using UnrealBuildTool;

public class FastGameUE : ModuleRules
{
	public FastGameUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FastGame"
		});
	}
}
