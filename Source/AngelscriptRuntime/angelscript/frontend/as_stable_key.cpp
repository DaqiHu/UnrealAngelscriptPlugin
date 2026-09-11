/*
   Maintained Unreal AngelScript fork — fixed public semantic identity value.
*/

#include "frontend/as_stable_key.h"

#include "HAL/UnrealMemory.h"
#include "Misc/Crc.h"

BEGIN_AS_NAMESPACE

bool asSStableKey::TryCreate(
		const TConstArrayView<uint8> Bytes,
		asSStableKey& OutKey)
	{
		OutKey = {};
		if (Bytes.Num() != static_cast<int32>(ByteCount))
		{
			return false;
		}
		FMemory::Memcpy(OutKey.Value, Bytes.GetData(), ByteCount);
		return true;
	}

	bool asSStableKey::IsZero() const
	{
		for (const uint8 Byte : Value)
		{
			if (Byte != 0)
			{
				return false;
			}
		}
		return true;
	}

	FString asSStableKey::ToHexString() const
	{
		static constexpr TCHAR Digits[] = TEXT("0123456789abcdef");
		FString Result;
		Result.Reserve(ByteCount * 2);
		for (const uint8 Byte : Value)
		{
			Result.AppendChar(Digits[Byte >> 4]);
			Result.AppendChar(Digits[Byte & 0x0f]);
		}
		return Result;
	}

	bool asSStableKey::operator==(const asSStableKey& Other) const
	{
		return FMemory::Memcmp(Value, Other.Value, ByteCount) == 0;
	}

	uint32 GetTypeHash(const asSStableKey& Key)
	{
		return FCrc::MemCrc32(Key.GetBytes().GetData(), asSStableKey::ByteCount);
	}

END_AS_NAMESPACE
