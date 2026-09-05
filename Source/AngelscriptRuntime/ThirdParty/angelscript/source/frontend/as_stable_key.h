/*
   Maintained Unreal AngelScript fork — fixed public semantic identity value.
*/

#pragma once

#include "as_config.h"

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

struct ANGELSCRIPTRUNTIME_API asSStableKey final
	{
		static constexpr asUINT ByteCount = 32;

		static bool TryCreate(TConstArrayView<uint8> Bytes, asSStableKey& OutKey);

		bool IsZero() const;
		FString ToHexString() const;
		TConstArrayView<uint8> GetBytes() const { return MakeArrayView(Value); }

		bool operator==(const asSStableKey& Other) const;
		bool operator!=(const asSStableKey& Other) const { return !(*this == Other); }

	private:
		uint8 Value[ByteCount] = {};
	};

	static_assert(sizeof(asSStableKey) == asSStableKey::ByteCount,
		"A stable identity is exactly one BLAKE3-256 value");

	ANGELSCRIPTRUNTIME_API uint32 GetTypeHash(const asSStableKey& Key);

END_AS_NAMESPACE
