#include "FUEAssetCache.h"

FUEAssetCache* FUEAssetCache::Get()
{
	if (!Instance)
	{
		Instance = new FUEAssetCache();
	}

	return Instance;
}

FUE4AssetData* FUEAssetCache::Get(const FString& FilePath)
{
	auto AssetData = FileCache.Find(FilePath);
		
	//Asset data doesn't exist in cache
	if (!AssetData)
	{
		return nullptr;
	}

	//Data exists in cache and we check if it is still relevant
	if (IFileManager::Get().GetTimeStamp(*FilePath) > AssetData->ModificationTime)
	{
		FileCache.Remove(FilePath);
		//We try to collect the garbage to close all uasset file handles
		TryCollectGarbage(RF_NoFlags, true);
		return nullptr;
	}

	//All data is still relevant and here we return it
	return &AssetData->AssetData;
}

void FUEAssetCache::Add(const FString& FilePath, const FUE4AssetData& Data)
{
	FUEAssetFileData AssetFileData;
	AssetFileData.AssetData = Data;
	AssetFileData.ModificationTime = IFileManager::Get().GetTimeStamp(*FilePath);
	FileCache.Add(FilePath, AssetFileData);
}

FUEAssetCache* FUEAssetCache::Instance = nullptr;
