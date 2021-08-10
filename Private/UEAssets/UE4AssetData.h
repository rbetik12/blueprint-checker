#pragma once

#include <vector>

#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "UE4AssetData.generated.h"

USTRUCT()
struct FBlueprintClassObject
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int Index;

	UPROPERTY()
	FString ObjectName;

	UPROPERTY()
	FString ClassName;

	UPROPERTY()
	FString SuperClassName;

	FBlueprintClassObject(const int Index, const FString ObjectName, const FString ClassName,
	                      const FString SuperClassName):
		Index(Index),
		ObjectName(ObjectName),
		ClassName(ClassName),
		SuperClassName(SuperClassName)
	{
	}

	FBlueprintClassObject(): Index(0)
	{
	}
};

UENUM()
enum class EKind
{
	VariableGet,
	VariableSet,
	FunctionCall,
	AddDelegate,
	ClearDelegate,
	CallDelegate,
	Other
};

USTRUCT()
struct FK2GraphNodeObject
{
	GENERATED_USTRUCT_BODY()

	static EKind GetKindByClassName(const FString& ClassName)
	{
		if (ClassName == "K2Node_CallFunction")
		{
			return EKind::FunctionCall;
		}
		if (ClassName == "K2Node_VariableSet")
		{
			return EKind::VariableSet;
		}
		if (ClassName == "K2Node_VariableGet")
		{
			return EKind::VariableGet;
		}
		if (ClassName == "K2Node_AddDelegate")
		{
			return EKind::AddDelegate;
		}
		if (ClassName == "K2Node_ClearDelegate")
		{
			return EKind::ClearDelegate;
		}
		if (ClassName == "K2Node_CallDelegate")
		{
			return EKind::CallDelegate;
		}
		return EKind::Other;
	}

	UPROPERTY()
	int Index;

	UPROPERTY()
	EKind ObjectKind;

	UPROPERTY()
	FString MemberName;

	FK2GraphNodeObject(const int Index, const EKind ObjectKind, const FString MemberName):
		Index(Index),
		ObjectKind(ObjectKind),
		MemberName(MemberName)
	{
	}

	FK2GraphNodeObject(): Index(0), ObjectKind(EKind::Other)
	{
	}
};

USTRUCT()
struct FOtherAssetObject
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int Index;

	UPROPERTY()
	FString ClassName;

	FOtherAssetObject(const int Index, const FString ClassName):
		Index(Index),
		ClassName(ClassName)
	{
	}

	FOtherAssetObject(): Index(0)
	{
	}
};

USTRUCT()
struct FUE4AssetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FBlueprintClassObject> BlueprintClasses;

	UPROPERTY()
	TArray<FK2GraphNodeObject> K2VariableSets;

	UPROPERTY()
	TArray<FOtherAssetObject> OtherClasses;
};
