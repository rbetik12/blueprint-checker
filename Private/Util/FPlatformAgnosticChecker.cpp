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
		UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Can't convert path %s to engine-friendly path"), BlueprintPath);
		return false;
	}
	
	const FString BlueprintFilename = FPaths::GetBaseFilename(BlueprintPath);
	const bool ParseResult = ParseBlueprint(EngineInternalPath, BlueprintFilename);
	const bool DeleteResult = DeleteCopiedUAsset(BlueprintFilename);

	if (DeleteResult)
	{
		UE_LOG(LogPlatformAgnosticChecker, Display, TEXT("Successfully deleted file: %s"), *BlueprintFilename);
	}
	else
	{
		UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Failed to delete file: %s"), *BlueprintFilename);
	}
	
	return ParseResult;
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
	IFileManager* FileManager = &IFileManager::Get();
	const FString EngineTempDirPath(FPaths::EngineSavedDir() + "BlueprintChecker/");
	
	if (!FileManager->DirectoryExists(*EngineTempDirPath))
	{
		if (!FileManager->MakeDirectory(*EngineTempDirPath))
		{
			UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Can't create directory %s"), *EngineTempDirPath);
			return false;	
		}
	}
	
	const FString DestFilePath = EngineTempDirPath + FPaths::GetCleanFilename(BlueprintPath);
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
	FLinkerLoad* Linker = nullptr;
	TRefCountPtr<FUObjectSerializeContext> LoadContext(FUObjectThreadContext::Get().GetSerializeContext());
	BeginLoad(LoadContext);
	{
		FUObjectSerializeContext* InOutLoadContext = LoadContext;
		Linker = GetPackageLinker(nullptr, *BlueprintInternalPath, 0x0, nullptr, nullptr, nullptr, &InOutLoadContext);
		if (InOutLoadContext != LoadContext)
		{
			// The linker already existed and was associated with another context
			LoadContext->DecrementBeginLoadCount();
			LoadContext = InOutLoadContext;
			LoadContext->IncrementBeginLoadCount();
		}
	}
	
	if (!Linker)
	{
		TryCollectGarbage(RF_NoFlags, false);
		UE_LOG(LogPlatformAgnosticChecker, Error, TEXT("Can't get package linker. Package: %s"), *BlueprintInternalPath);
		return false;
	}

	UE_LOG(LogPlatformAgnosticChecker, Display, TEXT("Successfully got package linker. Package: %s"), *BlueprintInternalPath);
	const bool Result = SerializeUAssetInfo(Linker, BlueprintFilename);
	EndLoad(Linker->GetSerializeContext());
	Linker = nullptr;
	TryCollectGarbage(RF_NoFlags, true);
	
	return Result;
}

bool FPlatformAgnosticChecker::SerializeUAssetInfo(FLinkerLoad* UAssetLinker, const FString& BlueprintFilename)
{
	IUEAssetSerializer* Serializer = IUEAssetSerializer::Create(UAssetLinker, BlueprintFilename);
	const bool Result = Serializer->Serialize();
	delete Serializer;
	return Result;
}

bool FPlatformAgnosticChecker::DeleteCopiedUAsset(const FString& BlueprintFilename)
{
	const FString EngineTempDirPath(FPaths::EngineSavedDir() + "BlueprintChecker/" + BlueprintFilename + ".uasset");
	IFileManager* FileManager = &IFileManager::Get();
	return FileManager->Delete(*FPaths::ConvertRelativePathToFull(EngineTempDirPath), false, true);
}

FString FPlatformAgnosticChecker::ConvertToEngineFriendlyPath(const TCHAR* BlueprintPath)
{
	// User can provide path with backslashes, so here we guarantee that it will be converted to normal path
	FString FullPath = FString(BlueprintPath).Replace(TEXT("\\"), TEXT("/"));

	//Checks whether this path contains content directory and uasset or umap in it
	if (FullPath.Find("/Content/") == INDEX_NONE || (FullPath.Find(".uasset") == INDEX_NONE && FullPath.Find(".umap") == INDEX_NONE))
	{
		return FString();
	}

	FString Filename = FPaths::GetBaseFilename(BlueprintPath);

	// Here we check whether this path points to plugin or game related content. If it is, we copy uassets from there to
	// engine temp directory
	if (FullPath.Find(TEXT("/Plugins/")) != INDEX_NONE || FullPath.Find(TEXT("/Engine/")) == INDEX_NONE)
	{
		if (CopyFileToContentDir(BlueprintPath))
		{
			return FString("/Temp/BlueprintChecker/") + Filename;	
		}
	}
	// If it's not a plugin or game related content, we parse engine path and extract from there /Content/ folder
	// It works like this: <user stuff>/Engine/Content/SomeDir/Dir/Asset.uasset -> /Engine/SomeDir/Dir/Asset
	else
	{
		TArray<FString> TokenizedPath;
		FullPath.ParseIntoArray(TokenizedPath, TEXT("/"), true);
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

	return FString();
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
