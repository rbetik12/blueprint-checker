#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <iostream>
#include "FEngineWorker.h"
#include <EdGraph/EdGraph.h>

bool FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	if (CopyFileToContentDir(BlueprintPath))
	{
		FString InternalPath = ConstructBlueprintInternalPath(BlueprintPath);
		const bool ParseResult = ParseBlueprint(InternalPath);

		if (ParseResult)
		{
			return true;
		}
	}

	return false;
}

void FPlatformAgnosticChecker::Exit()
{
	//TODO Delete temporary files and directories we created earlier
	FEngineWorker::Exit();
}

#if RUN_WITH_TESTS
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
	const uint32 CopyResult = FileManager->Copy(*DestFilePath, BlueprintPath, true, true, false, nullptr, FILEREAD_None, FILEWRITE_EvenIfReadOnly);

	if (CopyResult != COPY_OK)
	{
		std::wcerr << "Can't copy file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
		return false;
	}

	std::wcout << "Successfully copied file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
	return true;
}

bool FPlatformAgnosticChecker::ParseBlueprint(const FString& BlueprintInternalPath)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintInternalPath);
	if (!Blueprint)
	{
		return false;
	}
	const FString ParentClassName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
	const FString ObjectName = Blueprint->GetName();
	const FString ClassName = Blueprint->GetClass()->GetName();

	const BlueprintClassObject BPClassObject(0, ObjectName, ClassName, ParentClassName);
	AssetData.BlueprintClasses.push_back(BPClassObject);

	for (const auto& Graph: Blueprint->UbergraphPages)
	{
		for (const auto& Node: Graph->Nodes)
		{
			K2GraphNodeObject::Kind Kind = K2GraphNodeObject::GetKindByClassName(Node->GetClass()->GetName());
			if (Kind == K2GraphNodeObject::Kind::Other)
			{
				OtherAssetObject OtherAsset(0, Node->GetClass()->GetName());
				AssetData.OtherClasses.push_back(OtherAsset);
			}
			else
			{
				FString MemberName = Node->GetName();
				const K2GraphNodeObject K2Node(0, Kind, MemberName);
				AssetData.K2VariableSets.push_back(K2Node);	
			}
		}
	}
	
	return true;
}

FString FPlatformAgnosticChecker::ConstructBlueprintInternalPath(const TCHAR* BlueprintPath)
{
	const FString BlueprintInternalPath = FString("/Engine/_Temp/") + FPaths::GetBaseFilename(BlueprintPath);
	return BlueprintInternalPath;
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
UE4AssetData FPlatformAgnosticChecker::AssetData = UE4AssetData();


