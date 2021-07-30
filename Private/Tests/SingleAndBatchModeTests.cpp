#pragma once
#if RUN_WITH_TESTS

#include <gtest/gtest.h>
#include <array>
#include "FPlatformAgnosticChecker.h"

//A bit sketchy, but okay
bool RunMain(int Argc, char* Argv[]);

class SingleAndBatchModeTests: public ::testing::Test
{
	friend class FPlatformAgnosticChecker;
protected:
	static void SetUpTestSuite() 
	{
		FPlatformAgnosticChecker::Init();
	}
	
	std::array<FString, 1> BlueprintFilenames = {
		"NewBlueprint"
	};

	std::array<FString, 1> BatchFiles = {
		"batch.txt"
	};
};

TEST_F(SingleAndBatchModeTests, SingleModeTest)
{
	const int Argc = 3;
	char* Argv[Argc] = {
		"",
		"--mode=Single",
		"-f=C:/Users/Vitaliy/Code/UnrealEngine/Engine/Content/_TestUAssets/NewBlueprint.uasset"
	};
	
	const bool Result = RunMain(Argc, Argv);

	EXPECT_TRUE(Result);
}

#endif