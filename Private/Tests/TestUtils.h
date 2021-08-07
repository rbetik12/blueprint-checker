#pragma once
#include "Containers/UnrealString.h"

class TestUtils
{
public:
	static FString EnginePathToAbsolutePath(const FString& TestDirPath);
};
