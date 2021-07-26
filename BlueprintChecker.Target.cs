using System.Text;
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class BlueprintCheckerTarget : TargetRules
{
	public BlueprintCheckerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		LinkType = TargetLinkType.Modular;
		BuildEnvironment = TargetBuildEnvironment.Unique;
		LaunchModuleName = "BlueprintChecker";
		
		bBuildDeveloperTools = false;
		bIsBuildingConsoleApplication = true;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = true;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstApplicationCore = true;
		bBuildRequiresCookedData = false;
		bCompileWithStatsWithoutEngine = true;
	}
}