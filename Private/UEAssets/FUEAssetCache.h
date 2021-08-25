#pragma once
#include "UE4AssetData.h"
#include "Containers/Map.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"

struct FUEAssetFileData
{
	FDateTime ModificationTime;	
	FUE4AssetData AssetData;
};

class FUEAssetCache
{
public:
	static FUEAssetCache* Get();

	FUE4AssetData* Get(const FString& FilePath);

	void Add(const FString& FilePath, const FUE4AssetData& Data);

	//TODO Destructor
private:
	FUEAssetCache() = default;
	
	TMap<FString, FUEAssetFileData> FileCache;
	static FUEAssetCache* Instance;
};
