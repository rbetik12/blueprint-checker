#pragma once
#include "Misc/Paths.h"
#ifdef COMPILE_TESTS
#include <gtest/gtest.h>
#include <fstream>
#include "FPlatformAgnosticChecker.h"
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

std::vector<FString> FTests::UAssetFiles = std::vector<FString>();

#endif
