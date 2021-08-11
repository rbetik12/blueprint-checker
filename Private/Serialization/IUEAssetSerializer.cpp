#include "FJsonUEAssetSerializer.h"

IUEAssetSerializer* IUEAssetSerializer::Create(FLinkerLoad* Linker, const FString& Filename)
{
	return new FJsonUEAssetSerializer(Linker, Filename);
}