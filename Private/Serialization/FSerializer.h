#pragma once
#include "Containers/UnrealString.h"
#include "UEAssets/UE4AssetData.h"

class FSerializer
{
public:
	static bool SerializeFString(const FString& String, FILE* File);
	static bool SerializeInt32(const int32 Value, FILE* File);
	static bool SerializeInt64(const int64 Value, FILE* File);

	static bool SerializeBlueprintClassObject(const BlueprintClassObject& Obj, FILE* File);
	static bool SerializeK2GraphNodeObject(const K2GraphNodeObject& Obj, FILE* File);
	static bool SerializeOtherAssetObject(const OtherAssetObject& Obj, FILE* File);
};
