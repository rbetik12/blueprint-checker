#pragma once
#if RUN_WITH_TESTS

#include "FPlatformAgnosticChecker.h"
#include "FEngineWorker.h"

#include <gtest/gtest.h>
#include <array>

#define TEST_UASSETS_DIRECTORY "C:/Users/Vitaliy/Code/UnrealEngine/Engine/Content/_TestUAssets/"

class CopyFileTests: public ::testing::Test
{
	friend class FPlatformAgnosticChecker;
protected:
	static void SetUpTestSuite() 
	{
		FEngineWorker::Init();
	}

	static void TearDownTestSuite()
	{
		FEngineWorker::Exit();
	}
	
	//TODO Gather all files in TEST_UASSETS_DIRECTORY programmatically
	std::array<FString, 3> UAssetFilenames = {
		"NewBlueprint",
		"ThirdPersonCharacter",
		"ThirdPersonGameMode"
	};
};

TEST_F(CopyFileTests, ExistingUAssetCopyTest)
{
	for (auto Filename: UAssetFilenames)
	{
		const FString Filepath = FString(TEST_UASSETS_DIRECTORY) + Filename + ".uasset";
		const bool Result = FPlatformAgnosticChecker::CopyFileToContentDir(*Filepath);
		EXPECT_TRUE(Result);
	}
}

#endif