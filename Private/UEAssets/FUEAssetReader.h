#pragma once
#include "Containers/UnrealString.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectResource.h"

#define BLUEPRINT_GENERATED_CLASS_NAME "BlueprintGeneratedClass"
#define WIDGET_BLUEPRINT_GENERATED_CLASS "WidgetBlueprintGeneratedClass"

struct FObjectExportSerialized
{
	int Index;
	FString ObjectName;
	FString ClassName;
	FString SuperClassName;

	bool IsBlueprintGeneratedClass() const
	{
		return ClassName == BLUEPRINT_GENERATED_CLASS_NAME
			|| ClassName == WIDGET_BLUEPRINT_GENERATED_CLASS;
	}
};

class FUEAssetReader
{
public:
	explicit FUEAssetReader(FLinkerLoad* Linker);
	
	FObjectExportSerialized ReadObjectExport(const FObjectExport& ObjExport, int Index) const;
private:
	FLinkerLoad* Linker;
};
