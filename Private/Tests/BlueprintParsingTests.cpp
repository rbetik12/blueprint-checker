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
	
	std::array<FString, 3> BlueprintFilenames = {
		"Pickup_AmmoGun",
		"Pickup_AmmoLauncher",
		"TaskAlwaysTrue"
	};

	//TODO Add some nonblueprint uassets files here
	std::array<FString, 2> NonBlueprintFilenames = {
		"",
		""
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