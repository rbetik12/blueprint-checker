#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <iostream>
#include "FEngineWorker.h"

void FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	FEngineWorker::Init();
	if (CopyFileToContentDir(BlueprintPath))
	{
		ConstructBlueprintInternalPath(BlueprintPath);
		ParseBlueprint();
	}
	Exit();
}

void FPlatformAgnosticChecker::Exit()
{
	//TODO Delete temporary files and directories we created earlier
	FEngineWorker::Exit();
}

bool FPlatformAgnosticChecker::CopyFileToContentDir(const TCHAR* BlueprintPath)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "_Temp/_");
	const FString DestFilePath = EngineContentDirPath + FPaths::GetCleanFilename(BlueprintPath);
	
	IFileManager* FileManager = &IFileManager::Get();
	const uint32 CopyResult = FileManager->Copy(*DestFilePath, BlueprintPath, true);

	if (CopyResult != COPY_OK)
	{
		std::wcerr << "Can't copy file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
		return false;
	}

	std::wcout << "Successfully copied file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
	return true;
}

void FPlatformAgnosticChecker::ParseBlueprint()
{
	Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintInternalPath);
}

void FPlatformAgnosticChecker::ConstructBlueprintInternalPath(const TCHAR* BlueprintPath)
{
	const FString _BlueprintInternalPath = FString("/Engine/_Temp/") + FPaths::GetBaseFilename(BlueprintPath);
	BlueprintInternalPath = _BlueprintInternalPath;
}

UBlueprint* FPlatformAgnosticChecker::Blueprint = nullptr;
FString FPlatformAgnosticChecker::BlueprintInternalPath = FString();

