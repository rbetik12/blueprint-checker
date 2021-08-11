#pragma once
#include "UEAssets/UE4AssetData.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"

class IUEAssetSerializer
{
public:
	IUEAssetSerializer(FLinkerLoad* Linker, const FString& Filename): Linker(Linker), Filename(Filename)
	{
		FileManager = &IFileManager::Get();
	}
	
	virtual ~IUEAssetSerializer() = default;

	virtual bool Serialize() = 0;

	static FString Directory;
	
	static IUEAssetSerializer* Create(FLinkerLoad* Linker, const FString& Filename);

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

	virtual bool ParseExportMap() = 0;

	IFileManager* FileManager = nullptr;
	FLinkerLoad* Linker = nullptr;
	FString Filename;
};

// TODO cross-platform directory binding
FString IUEAssetSerializer::Directory = FWindowsPlatformProcess::UserDir() + FString("Temp/BlueprintChecker/");
