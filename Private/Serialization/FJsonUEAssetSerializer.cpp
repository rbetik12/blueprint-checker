#include "FJsonUEAssetSerializer.h"
#include "UEAssets/FUEAssetReader.h"
#include "JsonObjectConverter.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include <iostream>

DECLARE_LOG_CATEGORY_CLASS(LogFJsonUEAssetSerializer, Log, All);

bool FJsonUEAssetSerializer::Serialize()
{
	const bool ParseResult = ParseExportMap();

	if (ParseResult)
	{
		UE_LOG(LogFJsonUEAssetSerializer, Display, TEXT("Successfully parsed export map: %s"), *Filename);
	}
	else
	{
		UE_LOG(LogFJsonUEAssetSerializer, Error, TEXT("Failed to parse export map: %s"), *Filename);
		return false;
	}

	const bool JsonResult = FJsonObjectConverter::UStructToJsonObjectString(AssetData, JSONPayload, 0, 0);

	if (!JsonResult)
	{
		UE_LOG(LogFJsonUEAssetSerializer, Error, TEXT("Failed to serialize JSON: %s"), *Filename);
		return false;
	}

	UE_LOG(LogFJsonUEAssetSerializer, Display, TEXT("Successfully serialized JSON: %s"), *Filename);

	return Save();
}

// TODO Write tests for this
bool FJsonUEAssetSerializer::ParseExportMap()
{
	TArray<FBlueprintClassObject> BlueprintClassObjects;
	TArray<FK2GraphNodeObject> K2GraphNodeObjects;
	TArray<FOtherAssetObject> OtherAssetObjects;
	FUEAssetReader Reader(Linker);

	Linker->LoadAllObjects(false);
	for (int Index = 0; Index < Linker->ExportMap.Num(); Index++)
	{
		auto& ObjectExp = Linker->ExportMap[Index];
		FObjectExportSerialized ObjectExportSerialized = Reader.ReadObjectExport(ObjectExp, Index);

		if (ObjectExportSerialized.IsBlueprintGeneratedClass() && !ObjectExportSerialized.SuperClassName.IsEmpty())
		{
			BlueprintClassObjects.Add(FBlueprintClassObject(Index, ObjectExportSerialized.ObjectName,
			                                                ObjectExportSerialized.ClassName,
			                                                ObjectExportSerialized.SuperClassName));
		}
		else
		{
			const EKind Kind = FK2GraphNodeObject::GetKindByClassName(ObjectExportSerialized.ClassName);
			if (Kind != EKind::Other)
			{
				if (ObjectExp.Object)
				{
					const UK2Node* Node = Cast<UK2Node>(ObjectExp.Object);
					FString MemberName;
					FK2GraphNodeObject::GetMemberNameByClassName(Node, MemberName);

					K2GraphNodeObjects.Add(FK2GraphNodeObject(Index, Kind, MemberName));
				}
			}
			else
			{
				OtherAssetObjects.Add(FOtherAssetObject(Index, ObjectExportSerialized.ClassName));
			}
		}
	}
	
	AssetData.BlueprintClasses = BlueprintClassObjects;
	AssetData.OtherClasses = OtherAssetObjects;
	AssetData.K2VariableSets = K2GraphNodeObjects;
	
	return true;
}

bool FJsonUEAssetSerializer::Save()
{
	if (!GIsTestMode)
	{
		std::wcout << *JSONPayload << std::endl;
		std::wcout << "End" << std::endl;

		UE_LOG(LogFJsonUEAssetSerializer, Display, TEXT("Successfully written to stdout: %s"), *Filename);
	}

	return true;
}
