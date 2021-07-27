#pragma once
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "HAL/Platform.h"

class FPlatformAgnosticChecker
{
public:
	// User provides absolute path to blueprint and we must convert it to engine-friendly path
	static inline void Check(const TCHAR* BlueprintPath);
private:
	static bool CopyFileToContentDir(const TCHAR* BlueprintPath);
	static void ParseBlueprint();
	static void ConstructBlueprintInternalPath(const TCHAR* BlueprintPath);
	static void Exit();

	static FString BlueprintInternalPath;
	static UBlueprint* Blueprint;
};
