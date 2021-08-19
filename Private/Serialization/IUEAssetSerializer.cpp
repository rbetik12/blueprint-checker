#include "FBinaryUEAssetSerializer.h"
#include "FJsonUEAssetSerializer.h"

IUEAssetSerializer* IUEAssetSerializer::Create(FLinkerLoad* Linker, const FString& Filename, ESerializerType Type)
{
	switch (Type)
	{
	case ESerializerType::JSON:
		return new FJsonUEAssetSerializer(Linker, Filename);
	default:
		return new FBinaryUEAssetSerializer(Linker, Filename);
	}
}
