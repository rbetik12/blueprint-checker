#pragma once
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/ObjectResource.h"

class FPlatformAgnosticChecker
{
public:
	static void Init();
	static void Exit();
	
	// User provides absolute path to blueprint and we must convert it to engine-friendly path.
	// Contains all the logic for uasset parsing
	static bool Check(const TCHAR* BlueprintPath);
#if COMPILE_TESTS
	static void InitializeTestEnvironment(int Argc, char* Argv[]);
#endif

#if !COMPILE_TESTS
private:
#endif
	// Calls serialization routines for uasset. We provided linker as an argument, because it contains all necessary tables for Rider cache (e.g. Export Table)
	static bool SerializeUAssetInfo(FLinkerLoad* UAssetLinker, const FString& BlueprintFilename);

	// Copies uassets from provided path to known temp directory in Engine. Currently it's /Engine/Saved/BlueprintChecker.
	// We do that, because user can provide game-related uassets or plugin-related uassets and we don't know which plugins we have
	// or where is user's game is located
	static bool CopyFileToContentDir(const TCHAR* BlueprintPath);

	// Loads object and extracts its linker
	static bool ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename);

	// Delete copied uasset from engine temp directory
	static bool DeleteCopiedUAsset(const FString& BlueprintFilename);

	// Converts absolute path to engine-friendly path (/Engine/<Asset path> or /Temp/<Asset path>).
	// If fails return empty FString
	static FString ConvertToEngineFriendlyPath(const TCHAR* BlueprintPath);
	
	static bool bIsEngineInitialized;
};
