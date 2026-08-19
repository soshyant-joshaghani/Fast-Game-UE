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

		// Multiplayer: add sibling plugin charisma-ai/colyseus-unreal and call it from game code
		// using Catalog.GetGameServer + PrepareSession.ColyseusRoom (see Samples/SandboxMultiplayer).
	}
}
