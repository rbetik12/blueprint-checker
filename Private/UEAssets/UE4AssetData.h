#pragma once

#include <vector>

#include "Containers/UnrealString.h"

struct BlueprintClassObject
{
	int Index;
	FString ObjectName;
	FString ClassName;
	FString SuperClassName;

	BlueprintClassObject(const int Index, const FString ObjectName, const FString ClassName, const FString SuperClassName):
		ObjectName(ObjectName),
		ClassName(ClassName),
		SuperClassName(SuperClassName),
		Index(Index)
	{
	}
};

struct K2GraphNodeObject
{
	enum class Kind
	{
		VariableGet,
		VariableSet,
		FunctionCall,
		AddDelegate,
		ClearDelegate,
		CallDelegate,
		Other
	};

	static Kind GetKindByClassName(const FString& ClassName)
	{
		if (ClassName == "K2Node_CallFunction")
		{
			return Kind::FunctionCall;
		}
		if (ClassName == "K2Node_VariableSet")
		{
			return Kind::VariableSet;
		}
		if (ClassName == "K2Node_VariableGet")
		{
			return Kind::VariableGet;
		}
		if (ClassName == "K2Node_AddDelegate")
		{
			return Kind::AddDelegate;
		}
		if (ClassName == "K2Node_ClearDelegate")
		{
			return Kind::ClearDelegate;
		}
		if (ClassName == "K2Node_CallDelegate")
		{
			return Kind::CallDelegate;
		}
		return Kind::Other;
	}

	int Index;
	Kind ObjectKind;
	FString MemberName;

	K2GraphNodeObject(const int Index, const Kind ObjectKind, const FString MemberName):
		Index(Index),
		ObjectKind(ObjectKind),
		MemberName(MemberName)
	{
	}
};

struct OtherAssetObject
{
	int Index;
	FString ClassName;

	OtherAssetObject(const int Index, const FString ClassName):
		Index(Index),
		ClassName(ClassName)
	{
		
	}
};

class UE4AssetData
{
public:
	std::vector<BlueprintClassObject> BlueprintClasses;
	std::vector<K2GraphNodeObject> K2VariableSets;
	std::vector<OtherAssetObject> OtherClasses;
};
