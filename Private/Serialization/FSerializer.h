#pragma once
#include "Containers/UnrealString.h"

class FSerializer
{
public:
	static bool SerializeFString(const FString& String, FILE* File);
	static bool SerializeInt32(const int Value, FILE* File);
};
