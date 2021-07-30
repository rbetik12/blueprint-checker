#pragma once
#if RUN_WITH_TESTS

#include "FPlatformAgnosticChecker.h"

#include <gtest/gtest.h>
#include <array>

class CopyFileTests: public ::testing::Test
{
	friend class FPlatformAgnosticChecker;
protected:
	static void SetUpTestSuite() 
	{
		FPlatformAgnosticChecker::Init();
	}

	static void TearDownTestSuite()
	{
		FPlatformAgnosticChecker::Exit();
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

TEST_F(CopyFileTests, UnExistingUAssetCopyTest)
{
	for (auto Filename: UAssetFilenames)
	{
		const FString Filepath = FString(TEST_UASSETS_DIRECTORY) + Filename + "_" + ".uasset";
		const bool Result = FPlatformAgnosticChecker::CopyFileToContentDir(*Filepath);
		EXPECT_FALSE(Result);
	}
}

#endif