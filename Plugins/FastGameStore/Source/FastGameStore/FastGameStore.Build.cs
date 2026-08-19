using UnrealBuildTool;
using System.IO;

public enum FastGameStoreFlavor
{
	Myket,
	CafeBazaar,
	GooglePlay
}

public class FastGameStore : ModuleRules
{
	// One store per APK. Must match Initialize Game StorePlatform.
	private static FastGameStoreFlavor StoreFlavor = FastGameStoreFlavor.Myket;

	public FastGameStore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FastGame"
		});

		if (StoreFlavor == FastGameStoreFlavor.Myket)
		{
			PublicDefinitions.Add("FASTGAME_STORE_MYKET=1");
		}
		else if (StoreFlavor == FastGameStoreFlavor.CafeBazaar)
		{
			PublicDefinitions.Add("FASTGAME_STORE_CAFEBAZAAR=1");
		}
		else
		{
			PublicDefinitions.Add("FASTGAME_STORE_GOOGLEPLAY=1");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			if (StoreFlavor == FastGameStoreFlavor.CafeBazaar)
			{
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "FastGameStore_CafeBazaar_APL.xml"));
			}
			else if (StoreFlavor == FastGameStoreFlavor.GooglePlay)
			{
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "FastGameStore_GooglePlay_APL.xml"));
			}
			else
			{
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "FastGameStore_Myket_APL.xml"));
			}
		}
	}
}
