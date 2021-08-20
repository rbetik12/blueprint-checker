#pragma once
#ifdef COMPILE_TESTS
#include <gtest/gtest.h>
#include <fstream>
#include "Misc/Paths.h"
#include "Util/FPlatformAgnosticChecker.h"

class FTests : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		FPlatformAgnosticChecker::Init();
		GameAssetPath = "C:/Users/Vitaliy/Code/Unreal/ShooterGame/Content/Blueprints/TaskAlwaysTrue.uasset";
		EngineAssetPath = FPaths::EngineContentDir() + "Engine_MI_Shaders/Instances/M_ES_Phong_Opaque_INST_01.uasset";
		PluginAssetPath = FPaths::EnginePluginsDir() + "Compositing/Composure/Content/Materials/ChromaticAberration/ComposureChromaticAberrationSwizzling.uasset";
		IncorrectPluginAssetPathWithoutExtension = FPaths::EnginePluginsDir() + "Compositing/Composure/Content/Materials/ChromaticAberration/ComposureChromaticAberrationSwizzling";
		IncorrectPluginAssetPath = FPaths::EnginePluginsDir() + "Compositing/Composure/Materials/ChromaticAberration/ComposureChromaticAberrationSwizzling.uasset";
		ShaderModuleDependedAssetPath = FPaths::EngineContentDir() + "VREditor/VREditorAssetContainerData.uasset";
		BlueprintThatCrashedAssetPath = FPaths::EnginePluginsDir() + "VirtualProduction/DMX/DMXFixtures/Content/Fireworks/Blueprints/BP_FireWorksShell.uasset";
		UMapAssetPath = FPaths::EngineContentDir() + "Maps/Templates/TimeOfDay_Default.umap";
	}

	static void TearDownTestSuite()
	{
		FPlatformAgnosticChecker::Exit();
	}
	
	static FString GameAssetPath;
	static FString EngineAssetPath;
	static FString PluginAssetPath;
	static FString IncorrectPluginAssetPath;
	static FString IncorrectPluginAssetPathWithoutExtension;
	static FString ShaderModuleDependedAssetPath;
	static FString BlueprintThatCrashedAssetPath;
	static FString UMapAssetPath;
};

TEST_F(FTests, CopyFileTest)
{
	EXPECT_TRUE(FPlatformAgnosticChecker::CopyFileToContentDir(*PluginAssetPath));
}

TEST_F(FTests, CopyIncorrectFileTest)
{
	EXPECT_FALSE(FPlatformAgnosticChecker::CopyFileToContentDir(*IncorrectPluginAssetPathWithoutExtension));
}

TEST_F(FTests, ParseBlueprintTest)
{
	EXPECT_TRUE(FPlatformAgnosticChecker::Check(*PluginAssetPath));
}

TEST_F(FTests, ParseIncorrectBlueprintTest)
{
	EXPECT_FALSE(FPlatformAgnosticChecker::Check(*IncorrectPluginAssetPath));
}

TEST_F(FTests, TestGamePathConvertion)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*GameAssetPath);

	EXPECT_STREQ(TEXT("/Temp/BlueprintChecker/TaskAlwaysTrue"), *EngineFriendlyPath);
}

TEST_F(FTests, UMapPathConvertionTest)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*UMapAssetPath);

	EXPECT_STREQ(TEXT("/Engine/Maps/Templates/TimeOfDay_Default"), *EngineFriendlyPath);
}

TEST_F(FTests, TestEnginePathConvertion)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*EngineAssetPath);

	EXPECT_STREQ(TEXT("/Engine/Engine_MI_Shaders/Instances/M_ES_Phong_Opaque_INST_01"), *EngineFriendlyPath);
}

TEST_F(FTests, TestPluginPathConvertion)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*PluginAssetPath);

	EXPECT_STREQ(TEXT("/Temp/BlueprintChecker/ComposureChromaticAberrationSwizzling"), *EngineFriendlyPath);
}

TEST_F(FTests, TestIncorrectPathConvertionWithoutExtension)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*IncorrectPluginAssetPath);

	EXPECT_TRUE(EngineFriendlyPath.IsEmpty());
}

TEST_F(FTests, TestIncorrectPathConvertionWithoutContent)
{
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*IncorrectPluginAssetPathWithoutExtension);

	EXPECT_TRUE(EngineFriendlyPath.IsEmpty());
}

TEST_F(FTests, LoadSameUAssetTwice)
{
	// This test is related to issue, when I tried to get package linker for same uasset twice, program was just hanging up
	EXPECT_TRUE(FPlatformAgnosticChecker::Check(*EngineAssetPath));
	EXPECT_TRUE(FPlatformAgnosticChecker::Check(*EngineAssetPath));
}

TEST_F(FTests, TestShaderModuleDependedUAssetLoading)
{
	EXPECT_TRUE(FPlatformAgnosticChecker::Check(*ShaderModuleDependedAssetPath));
}

FString FTests::EngineAssetPath;
FString FTests::GameAssetPath;
FString FTests::PluginAssetPath;
FString FTests::IncorrectPluginAssetPath;
FString FTests::IncorrectPluginAssetPathWithoutExtension;
FString FTests::ShaderModuleDependedAssetPath;
FString FTests::BlueprintThatCrashedAssetPath;
FString FTests::UMapAssetPath;

#endif
