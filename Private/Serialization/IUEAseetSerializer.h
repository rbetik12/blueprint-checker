#pragma once
#include "UEAssets/UE4AssetData.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"

enum class ESerializerType
{
	JSON,
	Binary
};

// Abstract class that provides interface for serializing asset data
class IUEAssetSerializer
{
public:
	IUEAssetSerializer(FLinkerLoad* Linker, const FString& FilePath): Linker(Linker), FilePath(FilePath)
	{
		FileManager = &IFileManager::Get();
	}
	
	virtual ~IUEAssetSerializer() = default;


	// Overriding this method you should provide code for serializing the asset data (binary, JSON, etc)
	virtual bool Serialize() = 0;

	static FString Directory;
	
	static IUEAssetSerializer* Create(FLinkerLoad* Linker, const FString& Filename, ESerializerType Type = ESerializerType::Binary);

protected:
	virtual bool CreateSerializationDirectory()
	{
		if (!FileManager->DirectoryExists(*Directory))
		{
			FileManager->MakeDirectory(*Directory);
			
			if (FileManager->DirectoryExists(*Directory))
			{
				return false;
			}
		}
		return true;
	}

	// Overriding this method you should provide code for extracting data from UAsset Export map
	virtual bool ParseExportMap() = 0;

	// Overriding this method you should provide code for saving serialized data (writing to stdout or saving to file)
	virtual bool Save() = 0;

	IFileManager* FileManager = nullptr;
	FLinkerLoad* Linker = nullptr;
	FString FilePath;
};

// TODO cross-platform directory binding
FString IUEAssetSerializer::Directory = FWindowsPlatformProcess::UserDir() + FString("Temp/BlueprintChecker/");
