#include "FJsonUEAssetSerializer.h"
#include "UEAssets/FUEAssetReader.h"
#include "JsonObjectConverter.h"
#include "K2Node.h"
#include "Misc/FileHelper.h"

bool FJsonUEAssetSerializer::Serialize()
{
	ParseExportMap();
	
	FString JSONPayload;
	const bool JsonResult = FJsonObjectConverter::UStructToJsonObjectString(AssetData, JSONPayload, 0, 0);

	if (!JsonResult)
	{
		return false;
	}

	const FString TmpFilename = "~" + Filename;
	const FString TmpFilenamePath = Directory + TmpFilename;
	
	const bool SaveResult = FFileHelper::SaveStringToFile(JSONPayload, *TmpFilenamePath);

	if (!SaveResult)
	{
		return false;
	}

	const FString FullFilenamePath = Directory + Filename + ".json";
	
	return FileManager->Move(*FullFilenamePath, *TmpFilenamePath, true, true);
}

bool FJsonUEAssetSerializer::ParseExportMap()
{
	TArray<FBlueprintClassObject> BlueprintClassObjects;
	TArray<FK2GraphNodeObject> K2GraphNodeObjects;
	TArray<FOtherAssetObject> OtherAssetObjects;
	FUEAssetReader Reader(Linker);

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
				if (ObjectExp.Object != nullptr)
				{
					const UK2Node* Node = static_cast<UK2Node*>(ObjectExp.Object);

					if (Node)
					{
						FString MemberName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
						K2GraphNodeObjects.Add(FK2GraphNodeObject(Index, Kind, MemberName));
					}
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
