#pragma once
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/ObjectResource.h"

class FPlatformAgnosticChecker
{
public:
	static void Init();
	static void Exit();
	
	// User provides absolute path to blueprint and we must convert it to engine-friendly path
	static bool Check(const TCHAR* BlueprintPath);
#if COMPILE_TESTS
	static void InitializeTestEnvironment(int Argc, char* Argv[]);
#endif

#if !COMPILE_TESTS
private:
#endif
	static bool SerializeUAssetInfo(FLinkerLoad* UAssetLinker, const FString& BlueprintFilename);
	static bool CopyFileToContentDir(const TCHAR* BlueprintPath);
	static bool ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename);
	static bool DeleteCopiedUAsset(const FString& BlueprintFilename);
	static FString ConvertToEngineFriendlyPath(const TCHAR* BlueprintPath);
	
	static bool bIsEngineInitialized;
};
