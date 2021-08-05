﻿#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <iostream>
#include <stdio.h>
#include "FEngineWorker.h"
#include <EdGraph/EdGraph.h>

#include "Serialization/FSerializer.h"
#include "UEAssets/UE4AssetSerializedHeader.h"

bool FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	if (CopyFileToContentDir(BlueprintPath))
	{
		const FString InternalPath = ConstructBlueprintInternalPath(BlueprintPath);
		const FString BlueprintFilename = FPaths::GetBaseFilename(BlueprintPath);
		const bool ParseResult = ParseBlueprint(InternalPath, BlueprintFilename);

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

bool FPlatformAgnosticChecker::ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintInternalPath);
	if (!Blueprint)
	{
		return false;
	}
	UE4AssetData AssetData;
	const FString ParentClassName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
	const FString ObjectName = Blueprint->GetName();
	const FString ClassName = Blueprint->GetClass()->GetName();

	const BlueprintClassObject BPClassObject(0, ObjectName, ClassName, ParentClassName);
	AssetData.BlueprintClasses.push_back(BPClassObject);

	ExtractGraphInfo(Blueprint->UbergraphPages, AssetData);
	ExtractGraphInfo(Blueprint->FunctionGraphs, AssetData);
	ExtractGraphInfo(Blueprint->DelegateSignatureGraphs, AssetData);
	ExtractGraphInfo(Blueprint->MacroGraphs, AssetData);
	ExtractGraphInfo(Blueprint->IntermediateGeneratedGraphs, AssetData);
	ExtractGraphInfo(Blueprint->EventGraphs, AssetData);

	return SerializeBlueprintInfo(AssetData, BlueprintFilename);
}

bool FPlatformAgnosticChecker::SerializeBlueprintInfo(const UE4AssetData& AssetData, const FString& BlueprintFilename)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "_Temp/");
	const FString DestFilePath = EngineContentDirPath + BlueprintFilename + ".dat";

	FILE* SerializeFile = fopen(TCHAR_TO_UTF8(*DestFilePath), "wb");
	
	if (SerializeFile == nullptr)
	{
		return false;
	}

	bool SerializeStatus = true;
	
	UE4AssetSerializedHeader Header;
	Header.BlueprintClassesSize = AssetData.BlueprintClasses.size();
	Header.OtherClassesSize = AssetData.OtherClasses.size();
	Header.K2NodesSize = AssetData.K2VariableSets.size();

	const int WriteAmount = fwrite(&Header, sizeof(Header), 1, SerializeFile);

	if (WriteAmount < 1)
	{
		SerializeStatus = false;
	}

	for (auto& BlueprintClass: AssetData.BlueprintClasses)
	{
		if (!FSerializer::SerializeInt32(BlueprintClass.Index, SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeFString(BlueprintClass.ObjectName, SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeFString(BlueprintClass.ClassName, SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeFString(BlueprintClass.SuperClassName, SerializeFile))
		{
			SerializeStatus = false;
		}
	}

	for (auto& K2Node: AssetData.K2VariableSets)
	{
		if (!FSerializer::SerializeInt32(K2Node.Index, SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeInt32(static_cast<int>(K2Node.ObjectKind), SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeFString(K2Node.MemberName, SerializeFile))
		{
			SerializeStatus = false;
		}
	}

	for (auto& OtherAsset: AssetData.OtherClasses)
	{
		if (!FSerializer::SerializeInt32(OtherAsset.Index, SerializeFile))
		{
			SerializeStatus = false;
		}
		if (!FSerializer::SerializeFString(OtherAsset.ClassName, SerializeFile))
		{
			SerializeStatus = false;
		}
	}

	fclose(SerializeFile);
	
	return SerializeStatus;
}

FString FPlatformAgnosticChecker::ConstructBlueprintInternalPath(const TCHAR* BlueprintPath)
{
	const FString BlueprintInternalPath = FString("/Engine/_Temp/") + FPaths::GetBaseFilename(BlueprintPath);
	return BlueprintInternalPath;
}

void FPlatformAgnosticChecker::ExtractGraphInfo(const TArray<UEdGraph*> ExtractGraph, UE4AssetData& AssetData)
{
	for (const auto& Graph: ExtractGraph)
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


