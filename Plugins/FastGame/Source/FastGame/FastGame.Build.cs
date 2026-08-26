using UnrealBuildTool;

public class FastGame : ModuleRules
{
	public FastGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});

		// Multiplayer: Realtime.JoinMap (seat mint) + sibling Colyseus with seat_token.
		// Do not wrap Colyseus inside FastGame (see CONTRACT Realtime.JoinMap).
	}
}
