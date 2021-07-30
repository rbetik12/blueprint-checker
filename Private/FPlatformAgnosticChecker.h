#pragma once
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "HAL/Platform.h"

class FPlatformAgnosticChecker
{
public:
	static void Init();
	static void Exit();
	
	// User provides absolute path to blueprint and we must convert it to engine-friendly path
	static bool Check(const TCHAR* BlueprintPath);
	
#if RUN_WITH_TESTS
	static void InitializeTestEnvironment(int Argc, char* Argv[]);
#endif

#if !RUN_WITH_TESTS
private:
#endif
	static bool CopyFileToContentDir(const TCHAR* BlueprintPath);
	static UBlueprint* ParseBlueprint(const FString& BlueprintInternalPath);
	static FString ConstructBlueprintInternalPath(const TCHAR* BlueprintPath);
	
	static bool bIsEngineInitialized;
};
