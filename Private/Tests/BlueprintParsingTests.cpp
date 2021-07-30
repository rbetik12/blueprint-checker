#pragma once
#if RUN_WITH_TESTS

#include <gtest/gtest.h>
#include <array>
#include "FPlatformAgnosticChecker.h"

class BlueprintParsingTests: public ::testing::Test
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
	
	std::array<FString, 1> BlueprintFilenames = {
		"NewBlueprint"
	};

	std::array<FString, 2> NonBlueprintFilenames = {
		"ThirdPersonCharacter",
		"ThirdPersonGameMode"
	};
};

TEST_F(BlueprintParsingTests, BlueprintCheckTest)
{
	for (auto Filename: BlueprintFilenames)
	{
		const FString Filepath = FString(TEST_UASSETS_DIRECTORY) + Filename + ".uasset";
		const bool Result = FPlatformAgnosticChecker::Check(*Filepath);
		EXPECT_TRUE(Result);
	}
}

TEST_F(BlueprintParsingTests, NonBlueprintCheckTest)
{
	for (auto Filename: NonBlueprintFilenames)
	{
		const FString Filepath = FString(TEST_UASSETS_DIRECTORY) + Filename + ".uasset";
		const bool Result = FPlatformAgnosticChecker::Check(*Filepath);
		EXPECT_FALSE(Result);
	}
}

#endif