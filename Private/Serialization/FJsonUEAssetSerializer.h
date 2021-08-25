#pragma once
#include "IUEAseetSerializer.h"

extern bool GIsTestMode;

// Serializes uasset data to JSON and writes it to standard output
class FJsonUEAssetSerializer : public IUEAssetSerializer
{
public:
	FJsonUEAssetSerializer(FLinkerLoad* Linker, const FString& FilePath): IUEAssetSerializer(Linker, FilePath)
	{
	}

	virtual bool Serialize() override;
protected:
	virtual bool ParseExportMap() override;

	virtual bool Save() override;
	FUE4AssetData AssetData;
	FString JSONPayload;
};
