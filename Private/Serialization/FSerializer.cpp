#include "FSerializer.h"

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
