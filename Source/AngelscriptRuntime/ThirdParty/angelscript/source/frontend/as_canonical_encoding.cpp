/*
   Maintained Unreal AngelScript fork — canonical identity byte encoding.
*/

#include "frontend/as_canonical_encoding.h"

#include "Containers/StringConv.h"
#include "Hash/Blake3.h"

BEGIN_AS_NAMESPACE

asCCanonicalEncoding::asCCanonicalEncoding(
		asECanonicalIdentityDomain Domain,
		uint16 SchemaVersion)
	{
		static constexpr uint8 Prefix[] = {'A', 'S', 'I', 'D', 0};
		Bytes.Append(Prefix, UE_ARRAY_COUNT(Prefix));
		WriteUInt16(SchemaVersion);
		WriteUInt8(static_cast<uint8>(Domain));
	}

	void asCCanonicalEncoding::WriteUInt8(uint8 Value)
	{
		Bytes.Add(Value);
	}

	void asCCanonicalEncoding::WriteUInt16(uint16 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
	}

	void asCCanonicalEncoding::WriteUInt32(uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
		Bytes.Add(static_cast<uint8>(Value >> 16));
		Bytes.Add(static_cast<uint8>(Value >> 24));
	}

	void asCCanonicalEncoding::WriteUInt64(uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes.Add(static_cast<uint8>(Value >> Shift));
		}

	}

	void asCCanonicalEncoding::WriteBytes(TConstArrayView<uint8> Value)
	{
		WriteUInt32(static_cast<uint32>(Value.Num()));
		Bytes.Append(Value.GetData(), Value.Num());
	}

	void asCCanonicalEncoding::WriteString(FStringView Value)
	{
		const FTCHARToUTF8 Utf8(Value.GetData(), Value.Len());
		WriteUInt32(static_cast<uint32>(Utf8.Length()));
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	void asCCanonicalEncoding::WriteStringSequence(TConstArrayView<FString> Values)
	{
		WriteUInt32(static_cast<uint32>(Values.Num()));
		for (const FString& Value : Values) { WriteString(Value); }
	}

	asSStableKey asCCanonicalEncoding::FinalizeDigest() const
	{
		const FBlake3Hash Hash = FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
		asSStableKey Result;
		static_assert(asSStableKey::ByteCount == sizeof(FBlake3Hash::ByteArray));
		const bool bCreated = asSStableKey::TryCreate(
			MakeArrayView(Hash.GetBytes(), asSStableKey::ByteCount), Result);
		check(bCreated);
		return Result;
	}

	bool asCCanonicalEncoding::IsKnownDomain(asECanonicalIdentityDomain Domain)
	{
		return Domain == asECanonicalIdentityDomain::TypeDecl
			|| Domain == asECanonicalIdentityDomain::TypeUse
			|| Domain == asECanonicalIdentityDomain::Schema
			|| Domain == asECanonicalIdentityDomain::Layout
			|| Domain == asECanonicalIdentityDomain::Function
			|| Domain == asECanonicalIdentityDomain::Declaration;
	}

END_AS_NAMESPACE
