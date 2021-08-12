#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "FEngineWorker.h"
#include "JsonObjectConverter.h"
#include "K2Node.h"
#include "Engine/Engine.h"
#include "Serialization/IUEAseetSerializer.h"
#include "UObject/LinkerLoad.h"

DECLARE_LOG_CATEGORY_CLASS(LogPlatformAgnosticChecker, Log, All);

bool FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	const FString EngineInternalPath = ConvertToEngineFriendlyPath(BlueprintPath);
	if (EngineInternalPath.IsEmpty())
	{
		return false;
	}
	
	const FString BlueprintFilename = FPaths::GetBaseFilename(BlueprintPath);
	
	return ParseBlueprint(EngineInternalPath, BlueprintFilename);
}

void FPlatformAgnosticChecker::Exit()
{
	//TODO Delete temporary files and directories we created earlier
	FEngineWorker::Exit();
}

#if COMPILE_TESTS
#include <gtest/gtest.h>

void FPlatformAgnosticChecker::InitializeTestEnvironment(int Argc, char* Argv[])
{
	::testing::InitGoogleTest(&Argc, Argv);
	RUN_ALL_TESTS();
}
#endif

bool FPlatformAgnosticChecker::CopyFileToContentDir(const TCHAR* BlueprintPath)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "_Temp/");
	const FString DestFilePath = EngineContentDirPath + FPaths::GetCleanFilename(BlueprintPath);

	IFileManager* FileManager = &IFileManager::Get();
	const uint32 CopyResult = FileManager->Copy(*DestFilePath, BlueprintPath, true, false);

	if (CopyResult != COPY_OK)
	{
		UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Can't copy file from %s to %s"), BlueprintPath, *DestFilePath);
		return false;
	}

	UE_LOG(LogPlatformAgnosticChecker, Display, TEXT("Successfully copied file from %s to %s"), BlueprintPath,
	       *DestFilePath);
	return true;
}

bool FPlatformAgnosticChecker::ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename)
{
	UObject* Object = LoadObject<UObject>(nullptr, *BlueprintInternalPath);
	if (!Object)
	{
		TryCollectGarbage(RF_NoFlags, false);
		UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Can't load object %s"), *BlueprintInternalPath);
		return false;
	}

	auto LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	FLinkerLoad* Linker = GetPackageLinker(nullptr, *BlueprintInternalPath, 0x0, nullptr,
	                                       nullptr, nullptr, &LoadContext, nullptr, nullptr);

	UE_LOG(LogPlatformAgnosticChecker, Display, TEXT("Successfully Loaded object %s"), *BlueprintInternalPath);
	const bool Result = SerializeUAssetInfo(Linker, BlueprintFilename);
	Object = nullptr;
	TryCollectGarbage(RF_NoFlags, false);
	return Result;
}

bool FPlatformAgnosticChecker::SerializeUAssetInfo(FLinkerLoad* UAssetLinker, const FString& BlueprintFilename)
{
	IUEAssetSerializer* Serializer = IUEAssetSerializer::Create(UAssetLinker, BlueprintFilename);

	return Serializer->Serialize();
}

FString FPlatformAgnosticChecker::ConstructBlueprintInternalPath(const TCHAR* BlueprintPath)
{
	const FString BlueprintInternalPath = FString("/Engine/_Temp/") + FPaths::GetBaseFilename(BlueprintPath);
	return BlueprintInternalPath;
}

bool FPlatformAgnosticChecker::DeleteCopiedUAsset(const FString& BlueprintFilename)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "_Temp/" + BlueprintFilename + ".uasset");
	IFileManager* FileManager = &IFileManager::Get();
	return FileManager->Delete(*FPaths::ConvertRelativePathToFull(EngineContentDirPath), false, true);
}

FString FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(const TCHAR* BlueprintPath)
{
	//TODO Write some tests for this
	//Convert Windows path to normalized path
	FString FullPath = FString(BlueprintPath).Replace(TEXT("\\"), TEXT("/"));

	TArray<FString> TokenizedPath;
	FullPath.ParseIntoArray(TokenizedPath, TEXT("/"), true);

	FString Filename = FPaths::GetBaseFilename(BlueprintPath);
	TokenizedPath[TokenizedPath.Num() - 1] = Filename;

	FStringBuilderBase StringBuilder;
	bool AppendString = false;

	for (int64 Index = 0; Index < TokenizedPath.Num(); Index++)
	{
		if (AppendString)
		{
			StringBuilder.Append(TEXT("/"));
			StringBuilder.Append(TokenizedPath[Index]);
		}
		if (TokenizedPath[Index].Equals("Content"))
		{
			StringBuilder.Append(TEXT("/"));
			StringBuilder.Append(TokenizedPath[Index - 1]);
			AppendString = true;
		}
	}

	return StringBuilder.ToString();
}

void FPlatformAgnosticChecker::Init()
{
	if (!bIsEngineInitialized)
	{
		FEngineWorker::Init();
		bIsEngineInitialized = true;
	}
}

bool FPlatformAgnosticChecker::bIsEngineInitialized = false;
