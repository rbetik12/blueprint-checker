#pragma once
#include "Containers/UnrealString.h"
#include "UEAssets/UE4AssetData.h"

class FSerializer
{
public:
	static bool SerializeFString(const FString& String, FILE* File);
	static bool SerializeInt32(const int32 Value, FILE* File);
	static bool SerializeInt64(const int64 Value, FILE* File);

	static bool SerializeBlueprintClassObject(const FBlueprintClassObject& Obj, FILE* File);
	static bool SerializeK2GraphNodeObject(const FK2GraphNodeObject& Obj, FILE* File);
	static bool SerializeOtherAssetObject(const FOtherAssetObject& Obj, FILE* File);
	static bool SerializeUAssetDataToJson(const FUE4AssetData& AssetData, FILE* File);
};
