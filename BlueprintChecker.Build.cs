using UnrealBuildTool;

public class BlueprintChecker : ModuleRules {
	public BlueprintChecker(ReadOnlyTargetRules Target) : base(Target) {
		PrivateDefinitions.AddRange(new string[] {
		});

		PrivateIncludePaths.AddRange(new string[] {
			"Runtime/Launch/Private",
			"Runtime/CoreUObject/Private",
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
			"Launch"
		});
		
		PrivateIncludePathModuleNames.AddRange(new string[] {
			
		});
		
		PublicIncludePaths.AddRange(new string[] {
		});
	}
}