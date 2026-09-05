/*
   Maintained Unreal AngelScript fork — canonical identity byte encoding.
*/

#pragma once

#include "as_config.h"
#include "frontend/as_stable_key.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

enum class asECanonicalIdentityDomain : uint8
	{
		TypeDecl = 1,
		TypeUse = 2,
		Function = 6,
		Declaration = 7,
	};

	class ANGELSCRIPTRUNTIME_API asCCanonicalEncoding
	{
	public:
		static constexpr uint16 CurrentSchemaVersion = 1;

		explicit asCCanonicalEncoding(
			asECanonicalIdentityDomain Domain,
			uint16 SchemaVersion = CurrentSchemaVersion);

		void WriteUInt8(uint8 Value);
		void WriteUInt16(uint16 Value);
		void WriteUInt32(uint32 Value);
		void WriteUInt64(uint64 Value);
		void WriteBytes(TConstArrayView<uint8> Value);
		void WriteString(FStringView Value);
		void WriteStringSequence(TConstArrayView<FString> Values);

		TConstArrayView<uint8> GetBytes() const { return MakeArrayView(Bytes); }
		asSStableKey FinalizeDigest() const;

		static bool IsKnownDomain(asECanonicalIdentityDomain Domain);

	private:
		TArray<uint8> Bytes;
	};

END_AS_NAMESPACE
