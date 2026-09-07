/* Maintained Unreal AngelScript fork — authenticated schema and layout fingerprints. */
#include "as_metadata_fingerprint.h"

#include "HAL/UnrealMemory.h"
#include "Misc/Crc.h"

BEGIN_AS_NAMESPACE

namespace
{
	bool EqualBytes(TConstArrayView<uint8> Left, TConstArrayView<uint8> Right)
	{
		return Left.Num() == Right.Num()
			&& (Left.Num() == 0 || FMemory::Memcmp(Left.GetData(), Right.GetData(), Left.Num()) == 0);
	}

	asEFingerprintEquality CompareFingerprints(
		const asSFingerprintDigest& LeftHash,
		TConstArrayView<uint8> LeftWitness,
		const asSFingerprintDigest& RightHash,
		TConstArrayView<uint8> RightWitness)
	{
		if (LeftWitness.Num() == 0 || RightWitness.Num() == 0 || LeftHash.IsZero() || RightHash.IsZero())
		{
			return asEFingerprintEquality::Invalid;
		}
		const bool bSameWitness = EqualBytes(LeftWitness, RightWitness);
		const bool bSameHash = LeftHash == RightHash;
		if (bSameWitness && bSameHash)
		{
			return asEFingerprintEquality::Equal;
		}
		if (!bSameWitness && bSameHash)
		{
			return asEFingerprintEquality::Collision;
		}
		return asEFingerprintEquality::Different;
	}

}

bool asSFingerprintDigest::TryCreate(TConstArrayView<uint8> Bytes, asSFingerprintDigest& OutDigest)
{
	OutDigest = {};
	if (Bytes.Num() != static_cast<int32>(ByteCount))
	{
		return false;
	}
	FMemory::Memcpy(OutDigest.Value, Bytes.GetData(), ByteCount);
	return true;
}

bool asSFingerprintDigest::IsZero() const
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

FString asSFingerprintDigest::ToHexString() const
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

bool asSFingerprintDigest::operator==(const asSFingerprintDigest& Other) const
{
	return FMemory::Memcmp(Value, Other.Value, ByteCount) == 0;
}

asEFingerprintEquality CompareSchemaFingerprints(
	const asSSchemaHash& LeftHash,
	TConstArrayView<uint8> LeftWitness,
	const asSSchemaHash& RightHash,
	TConstArrayView<uint8> RightWitness)
{
	return CompareFingerprints(LeftHash.Digest, LeftWitness, RightHash.Digest, RightWitness);
}

asEFingerprintEquality CompareLayoutFingerprints(
	const asSLayoutHash& LeftHash,
	TConstArrayView<uint8> LeftWitness,
	const asSLayoutHash& RightHash,
	TConstArrayView<uint8> RightWitness)
{
	return CompareFingerprints(LeftHash.Digest, LeftWitness, RightHash.Digest, RightWitness);
}

uint32 GetTypeHash(const asSFingerprintDigest& Digest)
{
	return FCrc::MemCrc32(Digest.GetBytes().GetData(), asSFingerprintDigest::ByteCount);
}

END_AS_NAMESPACE
