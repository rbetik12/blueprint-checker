﻿#include "FSerializer.h"

bool FSerializer::SerializeFString(const FString& String, FILE* File)
{
	const size_t StringSize = String.Len();
	int WriteAmount = fwrite(&StringSize, sizeof(StringSize), 1, File);

	if (WriteAmount < 1)
	{
		return false;
	}

	WriteAmount = fwrite(*String, sizeof(TCHAR), StringSize, File);

	if (WriteAmount < StringSize)
	{
		return false;
	}

	return true;
}

bool FSerializer::SerializeInt32(const int Value, FILE* File)
{
	const int WriteAmount = fwrite(&Value, sizeof(Value), 1, File);

	if (WriteAmount < 1)
	{
		return false;
	}

	return true;
}

bool FSerializer::SerializeInt64(const int64 Value, FILE* File)
{
	const int WriteAmount = fwrite(&Value, sizeof(Value), 1, File);

	if (WriteAmount < 1)
	{
		return false;
	}

	return true;
}

bool FSerializer::SerializeBlueprintClassObject(const BlueprintClassObject& Obj, FILE* File)
{
	bool SerializeStatus = true;
	if (!SerializeInt32(Obj.Index, File))
	{
		SerializeStatus = false;
	}
	if (!SerializeFString(Obj.ObjectName, File))
	{
		SerializeStatus = false;
	}
	if (!SerializeFString(Obj.ClassName, File))
	{
		SerializeStatus = false;
	}
	if (!SerializeFString(Obj.SuperClassName, File))
	{
		SerializeStatus = false;
	}

	return SerializeStatus;
}

bool FSerializer::SerializeK2GraphNodeObject(const K2GraphNodeObject& Obj, FILE* File)
{
	bool SerializeStatus = true;
	if (!SerializeInt32(Obj.Index, File))
	{
		SerializeStatus = false;
	}
	if (!SerializeInt32(static_cast<int>(Obj.ObjectKind), File))
	{
		SerializeStatus = false;
	}
	if (!SerializeFString(Obj.MemberName, File))
	{
		SerializeStatus = false;
	}

	return SerializeStatus;
}

bool FSerializer::SerializeOtherAssetObject(const OtherAssetObject& Obj, FILE* File)
{
	bool SerializeStatus = true;
	if (!SerializeInt32(Obj.Index, File))
	{
		SerializeStatus = false;
	}
	if (!SerializeFString(Obj.ClassName, File))
	{
		SerializeStatus = false;
	}

	return SerializeStatus;
}
