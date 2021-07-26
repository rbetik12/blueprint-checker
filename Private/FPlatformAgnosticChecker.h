#pragma once
#include "HAL/Platform.h"

class FPlatformAgnosticChecker
{
public:
	// User provides absolute path to blueprint and we must convert it to engine-friendly path
	static inline void Check(const TCHAR* BlueprintPath);
private:
	static inline void CopyFileToContentDir(const TCHAR* BlueprintPath);
};
