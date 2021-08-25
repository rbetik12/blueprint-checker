#pragma once
#include "FJsonUEAssetSerializer.h"

//TODO Add cross-platform setmode
#include <io.h>
#include <fcntl.h>

// Serializes uasset data to binary format and writes it to standard output
class FBinaryUEAssetSerializer : public FJsonUEAssetSerializer
{
public:
	FBinaryUEAssetSerializer(FLinkerLoad* Linker, const FString& FilePath, FILE* OutputStream = stdout):
		FJsonUEAssetSerializer(Linker, FilePath), OutputStream(OutputStream)
	{
		_setmode( _fileno( stdout ),  _O_BINARY );
	}

	virtual ~FBinaryUEAssetSerializer() override
	{
		if (OutputStream != stdout)
		{
			fclose(OutputStream);
		}
	}

	virtual bool Serialize() override;
protected:
	virtual bool Save() override;
private:
	bool WriteFString(const FString& Value);
	bool WriteInt32(const int32 Value);
	bool WriteInt64(const int64 Value);
	bool FlushStream();
	FILE* OutputStream;
};
