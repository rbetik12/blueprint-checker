#include "FBinaryUEAssetSerializer.h"

DECLARE_LOG_CATEGORY_CLASS(LogFBinaryUEAssetSerializer, Log, All);

bool FBinaryUEAssetSerializer::Serialize()
{
	const bool ParseResult = ParseExportMap();

	return ParseResult && Save();
}

bool FBinaryUEAssetSerializer::Save()
{
	if (!GIsTestMode)
	{
		WriteInt32(AssetData.BlueprintClasses.Num());
		for (auto& Obj: AssetData.BlueprintClasses)
		{
			WriteInt32(Obj.Index);
			WriteFString(Obj.ObjectName);
			WriteFString(Obj.ClassName);
			WriteFString(Obj.SuperClassName);
		}
	
		WriteInt32(AssetData.K2VariableSets.Num());
		for (auto& Obj: AssetData.K2VariableSets)
		{
			WriteInt32(Obj.Index);
			WriteInt32(static_cast<int32>(Obj.ObjectKind));
			WriteFString(Obj.MemberName);
		}

		WriteInt32(AssetData.OtherClasses.Num());
		for (auto& Obj: AssetData.OtherClasses)
		{
			WriteInt32(Obj.Index);
			WriteFString(Obj.ClassName);
		}
	
		FlushStream();
	}

	UE_LOG(LogFBinaryUEAssetSerializer, Display, TEXT("Successfully written binary data to stdout: %s"), *FilePath);
	return true;
}

bool FBinaryUEAssetSerializer::WriteFString(const FString& Value)
{
	WriteInt64(Value.Len());
	fwrite(*Value, sizeof(TCHAR), Value.Len(), OutputStream);
	return true;
}

bool FBinaryUEAssetSerializer::WriteInt32(const int32 Value)
{
	const size_t Written = fwrite(&Value, sizeof(int32), 1, OutputStream);
	return true;
}

bool FBinaryUEAssetSerializer::WriteInt64(const int64 Value)
{
	const size_t Written = fwrite(&Value, sizeof(int64), 1, OutputStream);
	return true;
}

bool FBinaryUEAssetSerializer::FlushStream()
{
	fflush(OutputStream);
	return true;
}
