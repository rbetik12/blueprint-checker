#pragma once
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "HAL/Platform.h"
#include "UEAssets/UE4AssetData.h"

class FPlatformAgnosticChecker
{
public:
	static void Init();
	static void Exit();
	
	// User provides absolute path to blueprint and we must convert it to engine-friendly path
	static bool Check(const TCHAR* BlueprintPath);

	static bool SerializeBlueprintInfo(const UE4AssetData& AssetData, const FString& BlueprintFilename);
#if RUN_WITH_TESTS
	static void InitializeTestEnvironment(int Argc, char* Argv[]);
#endif

#if !RUN_WITH_TESTS
private:
#endif
	static bool CopyFileToContentDir(const TCHAR* BlueprintPath);
	static bool ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename);
	static FString ConstructBlueprintInternalPath(const TCHAR* BlueprintPath);
	static void ExtractGraphInfo(const TArray<UEdGraph*> Graph, UE4AssetData& AssetData);
	static bool DeleteCopiedUAsset(const FString& BlueprintFilename);
	static bool bIsEngineInitialized;
};
