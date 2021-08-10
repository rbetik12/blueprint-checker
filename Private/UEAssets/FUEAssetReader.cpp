#include "FUEAssetReader.h"

FUEAssetReader::FUEAssetReader(FLinkerLoad* Linker)
{
	this->Linker = Linker;
}

FObjectExportSerialized FUEAssetReader::ReadObjectExport(const FObjectExport& ObjExport, const int Index) const
{
	FObjectExportSerialized ObjectExportSerialized;
	ObjectExportSerialized.Index = Index;
	ObjectExportSerialized.ObjectName = ObjExport.ObjectName.ToString();
	ObjectExportSerialized.ClassName = Linker->ImpExp(ObjExport.ClassIndex).ObjectName.ToString();

	if (ObjExport.SuperIndex.IsNull())
	{
		ObjectExportSerialized.SuperClassName = FString();
	}
	else
	{
		ObjectExportSerialized.SuperClassName = Linker->ImpExp(ObjExport.SuperIndex).ObjectName.ToString();
	}

	return ObjectExportSerialized;
}
