/* Maintained Unreal AngelScript fork — versioned symbolic bytecode codec. */
#pragma once

#include "as_bytecode_image.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCByteCodeImageCodec
{
public:
	static constexpr uint16 CurrentVersion = 3;

	static asEByteCodeImageStatus Encode(const asCByteCodeImage& Image, TArray<uint8>& OutBytes);
	static asEByteCodeImageStatus Decode(TConstArrayView<uint8> Bytes, TUniquePtr<asCByteCodeImage>& OutImage);
	static asEByteCodeImageStatus Dump(const asCByteCodeImage& Image, FString& OutText);
};

END_AS_NAMESPACE
