#pragma once
#include "IUEAseetSerializer.h"

class FJsonUEAssetSerializer : public IUEAssetSerializer
{
public:
	FJsonUEAssetSerializer(FLinkerLoad* Linker, const FString& Filename): IUEAssetSerializer(Linker, Filename)
	{
	}

	virtual bool Serialize() override;
protected:
	virtual bool ParseExportMap() override;
	FUE4AssetData AssetData;
};
