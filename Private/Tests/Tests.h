#pragma once
#ifdef COMPILE_TESTS
#include <gtest/gtest.h>
#include <fstream>
#include "Misc/Paths.h"
#include "Util/FPlatformAgnosticChecker.h"
#include "TestUtils.h"

class FTests : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		FPlatformAgnosticChecker::Init();
		GetUAssetPaths();
	}

	static void TearDownTestSuite()
	{
		FPlatformAgnosticChecker::Exit();
	}

	static void GetUAssetPaths()
	{
		const FString EngineTestDir = TestUtils::EnginePathToAbsolutePath(TEST_UASSETS_DIRECTORY);
		const FString BatchFilePath = EngineTestDir + "batch.txt";

		std::ifstream FileStream(*BatchFilePath);

		if (!FileStream.good())
		{
			std::wcerr << "Can't open batch file!" << std::endl;
			FileStream.close();
			return;
		}

		std::string Path;
		while (std::getline(FileStream, Path))
		{
			UAssetFiles.push_back(Path.c_str());
		}
	}

	static std::vector<FString> UAssetFiles;
};

TEST_F(FTests, CopyFilesTest)
{
	for (auto& Path: UAssetFiles)
	{
		const FString AbsoluteBlueprintPath = FPaths::ConvertRelativePathToFull(Path);
		EXPECT_TRUE(FPlatformAgnosticChecker::CopyFileToContentDir(*AbsoluteBlueprintPath));
	}
}

TEST_F(FTests, BlueprintParsingTest)
{
	for (auto& Path: UAssetFiles)
	{
		const FString AbsoluteBlueprintPath = FPaths::ConvertRelativePathToFull(Path);
		EXPECT_TRUE(FPlatformAgnosticChecker::Check(*AbsoluteBlueprintPath));
	}
}

TEST_F(FTests, TestGamePathConvertion)
{
	const FString GameAssetPath = "C:/Users/Vitaliy/Code/Unreal/ShooterGame/Content/Blueprints/TaskAlwaysTrue.uasset";
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*GameAssetPath);

	EXPECT_STREQ(TEXT("/Temp/BlueprintChecker/TaskAlwaysTrue"), *EngineFriendlyPath);
}

TEST_F(FTests, TestEnginePathConvertion)
{
	const FString EngineAssetPath = FPaths::EngineContentDir() + "Engine_MI_Shaders/Instances/M_ES_Phong_Opaque_INST_01.uasset";
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*EngineAssetPath);

	EXPECT_STREQ(TEXT("/Engine/Engine_MI_Shaders/Instances/M_ES_Phong_Opaque_INST_01"), *EngineFriendlyPath);
}

TEST_F(FTests, TestPluginPathConvertion)
{
	const FString PluginAssetPath = FPaths::EnginePluginsDir() + "Compositing/Composure/Content/Materials/ChromaticAberration/ComposureChromaticAberrationSwizzling.uasset";
	const FString EngineFriendlyPath = FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(*PluginAssetPath);

	EXPECT_STREQ(TEXT("/Temp/BlueprintChecker/ComposureChromaticAberrationSwizzling"), *EngineFriendlyPath);
}

std::vector<FString> FTests::UAssetFiles = std::vector<FString>();

#endif
