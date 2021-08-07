#include "TestUtils.h"

#include "Misc/Paths.h"

FString TestUtils::EnginePathToAbsolutePath(const FString& TestDirPath)
{
	const FString EngineDirPath = FPaths::EngineDir() + TestDirPath;
	return EngineDirPath;
}
