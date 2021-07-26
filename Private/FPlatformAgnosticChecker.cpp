#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include <iostream>
#include "FEngineWorker.h"

void FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	FEngineWorker::Init();
	CopyFileToContentDir(BlueprintPath);
	FEngineWorker::Exit();
}

void FPlatformAgnosticChecker::CopyFileToContentDir(const TCHAR* BlueprintPath)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "BCTemp");

	std::wcout << (ToCStr(EngineContentDirPath)) << std::endl;
}

