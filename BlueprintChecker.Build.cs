using System.IO;
using UnrealBuildTool;

public class BlueprintChecker : ModuleRules {
	public BlueprintChecker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.AddRange(new string[]
		{
			"CUSTOM_ENGINE_INITIALIZATION=1",
			"COMPILE_TESTS=1",
			// That directory path must be provided relative to <UE root directory>/Engine/
			"TEST_UASSETS_DIRECTORY=\"Content/_TestUAssets/\"",
			"PROGRAM_VERSION=\"0.1\"",
			"PROGRAM_DESCRIPTION=\"Blueprint deserializer\""
		});

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
		}

		PrivateIncludePaths.AddRange(new string[] {
			"Runtime/Launch/Private",
			"Runtime/CoreUObject/Private",
			"Runtime/Core/Private",
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
			"GoogleTest",
			"Json",
			"JsonUtilities"
		});
	}
}