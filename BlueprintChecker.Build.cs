using System.IO;
using UnrealBuildTool;

public class BlueprintChecker : ModuleRules {
	public BlueprintChecker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.AddRange(new string[]
		{
			"CUSTOM_ENGINE_INITIALIZATION=1",
			// "RUN_WITH_TESTS=1",
			"TEST_UASSETS_DIRECTORY=\"C:/Users/Vitaliy/Code/UnrealEngine/Engine/Content/_TestUAssets/\"",
			"PROGRAM_VERSION=\"0.1\"",
			"PROGRAM_DESCRIPTION=\"Blueprint deserializer\""
		});

		PrivateIncludePaths.AddRange(new string[] {
			"Runtime/Launch/Private",
			"Runtime/CoreUObject/Private",
			Path.Combine(ModuleDirectory, "Private/ThirdParty")
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"Projects",
			"CoreUObject",
			"Engine",
			"DerivedDataCache",
			"MediaUtils",
			"ApplicationCore",
			"HeadMountedDisplay",
			"SlateRHIRenderer",
			"SlateNullRenderer",
			"MoviePlayer",
			"PreLoadScreen",
			"InstallBundleManager",
			"TaskGraph",
			"AutomationWorker",
			"SessionServices",
			"ProfilerService",
			"AutomationController",
			"PIEPreviewDeviceProfileSelector",
			"DesktopPlatform",
			"UnrealEd",
			"RenderCore",
			"SlateCore",
			"Slate",
			"RHI",
			"InputCore",
			"Launch",
			"GoogleTest"
		});
	}
}