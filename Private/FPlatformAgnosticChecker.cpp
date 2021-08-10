﻿#include "FPlatformAgnosticChecker.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <iostream>
#include <stdio.h>
#include "FEngineWorker.h"
#include <EdGraph/EdGraph.h>

#include "JsonObjectConverter.h"
#include "K2Node.h"
#include "Engine/Engine.h"
#include "Serialization/FSerializer.h"
#include "UEAssets/FUEAssetReader.h"
#include "UEAssets/UE4AssetSerializedHeader.h"
#include "UObject/LinkerLoad.h"

bool FPlatformAgnosticChecker::Check(const TCHAR* BlueprintPath)
{
	if (CopyFileToContentDir(BlueprintPath))
	{
		const FString InternalPath = ConstructBlueprintInternalPath(BlueprintPath);
		const FString BlueprintFilename = FPaths::GetBaseFilename(BlueprintPath);
		const bool ParseResult = ParseBlueprint(InternalPath, BlueprintFilename);
		const bool DeleteResult = DeleteCopiedUAsset(BlueprintFilename);
		if (ParseResult && DeleteResult)
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
		std::wcerr << "Can't copy file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
		return false;
	}

	std::wcout << "Successfully copied file from " << BlueprintPath << " to " << *DestFilePath << std::endl;
	return true;
}

bool FPlatformAgnosticChecker::ParseBlueprint(const FString& BlueprintInternalPath, const FString& BlueprintFilename)
{
	UObject* Object = LoadObject<UObject>(nullptr, *BlueprintInternalPath);
	if (!Object)
	{
		TryCollectGarbage(RF_NoFlags, false);
		std::wcerr << "Can't load object " << *BlueprintInternalPath << std::endl;
		return false;
	}

	// UE4AssetData AssetData;
	// if (Object->GetClass() == UBlueprint::StaticClass())
	// {
	// 	const UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	// 	const FString ParentClassName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
	// 	const FString ObjectName = Blueprint->GetName();
	// 	const FString ClassName = Blueprint->GetClass()->GetName();
	//
	// 	const BlueprintClassObject BPClassObject(0, ObjectName, ClassName, ParentClassName);
	// 	AssetData.BlueprintClasses.push_back(BPClassObject);
	//
	// 	ExtractGraphInfo(Blueprint->UbergraphPages, AssetData);
	// 	ExtractGraphInfo(Blueprint->FunctionGraphs, AssetData);
	// 	ExtractGraphInfo(Blueprint->DelegateSignatureGraphs, AssetData);
	// 	ExtractGraphInfo(Blueprint->MacroGraphs, AssetData);
	// 	ExtractGraphInfo(Blueprint->IntermediateGeneratedGraphs, AssetData);
	// 	ExtractGraphInfo(Blueprint->EventGraphs, AssetData);
	// }
	// else
	// {
	// 	OtherAssetObject OtherAsset(0, Object->GetClass()->GetName());
	// 	AssetData.OtherClasses.push_back(OtherAsset);
	// }

	auto LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	FLinkerLoad* Linker = GetPackageLinker(nullptr, *BlueprintInternalPath, 0x0, nullptr,
	                                       nullptr, nullptr, &LoadContext, nullptr, nullptr);

	std::wcout << "Successfully Loaded object " << *BlueprintInternalPath << std::endl;
	const bool Result = SerializeUAssetInfo(Linker, BlueprintFilename);
	Object = nullptr;
	TryCollectGarbage(RF_NoFlags, false);
	return Result;
}

bool FPlatformAgnosticChecker::SerializeUAssetInfo(FLinkerLoad* UAssetLinker, const FString& BlueprintFilename)
{
	FILE* SerializationFile = CreateSerializationFile(BlueprintFilename);

	if (SerializationFile == nullptr)
	{
		return false;
	}

	const bool ExportSerializeResult = SerializeExportMap(UAssetLinker, SerializationFile);

	fclose(SerializationFile);

	return ExportSerializeResult;
}

FILE* FPlatformAgnosticChecker::CreateSerializationFile(const FString& BlueprintFilename)
{
	const FString EngineContentDirPath(FPaths::EngineContentDir() + "_Temp/");
	const FString DestFilePath = EngineContentDirPath + BlueprintFilename + ".dat";

	FILE* SerializeFile = fopen(TCHAR_TO_UTF8(*DestFilePath), "wb");

	if (SerializeFile == nullptr)
	{
		std::wcerr << "Can't open " << *DestFilePath << " for serialization" << std::endl;
	}

	return SerializeFile;
}

bool FPlatformAgnosticChecker::SerializeExportMap(FLinkerLoad* UAssetLinker, FILE* File)
{
	TArray<FBlueprintClassObject> BlueprintClassObjects;
	TArray<FK2GraphNodeObject> K2GraphNodeObjects;
	TArray<FOtherAssetObject> OtherAssetObjects;
	FUEAssetReader Reader(UAssetLinker);

	for (int Index = 0; Index < UAssetLinker->ExportMap.Num(); Index++)
	{
		auto& ObjectExp = UAssetLinker->ExportMap[Index];
		FObjectExportSerialized ObjectExportSerialized = Reader.ReadObjectExport(ObjectExp, Index);

		if (ObjectExportSerialized.IsBlueprintGeneratedClass() && !ObjectExportSerialized.SuperClassName.IsEmpty())
		{
			BlueprintClassObjects.Add(FBlueprintClassObject(Index, ObjectExportSerialized.ObjectName,
			                                               ObjectExportSerialized.ClassName,
			                                               ObjectExportSerialized.SuperClassName));
		}
		else
		{
			const EKind Kind = FK2GraphNodeObject::GetKindByClassName(ObjectExportSerialized.ClassName);
			if (Kind != EKind::Other)
			{
				if (ObjectExp.Object != nullptr)
				{
					const UK2Node* Node = static_cast<UK2Node*>(ObjectExp.Object);

					if (Node)
					{
						FString MemberName = Node->GetName();
						K2GraphNodeObjects.Add(FK2GraphNodeObject(Index, Kind, MemberName));
					}
				}
			}
			else
			{
				OtherAssetObjects.Add(FOtherAssetObject(Index, ObjectExportSerialized.ClassName));
			}
		}
	}

	FUE4AssetData AssetData;
	AssetData.BlueprintClasses = BlueprintClassObjects;
	AssetData.OtherClasses = OtherAssetObjects;
	AssetData.K2VariableSets = K2GraphNodeObjects;
	
	FSerializer::SerializeUAssetDataToJson(AssetData, File);
	return true;
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

void FPlatformAgnosticChecker::Init()
{
	if (!bIsEngineInitialized)
	{
		FEngineWorker::Init();
		bIsEngineInitialized = true;
	}
}

bool FPlatformAgnosticChecker::bIsEngineInitialized = false;
