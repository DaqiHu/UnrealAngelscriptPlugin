/* Maintained Unreal AngelScript fork — authenticated schema and layout fingerprints. */
#pragma once

#include "as_config.h"
#include "frontend/as_stable_key.h"

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

class asCTypeInfo;
class asCMetadataImage;

enum class asEFingerprintStatus : asBYTE
{
	Succeeded,
	NotReady,
	UnauthenticatedDefinition,
	InvalidDefinition,
	UnsupportedTarget
};

enum class asEFingerprintEquality : asBYTE
{
	Equal,
	Different,
	Collision,
	Invalid
};

struct ANGELSCRIPTRUNTIME_API asSFingerprintDigest
{
	static constexpr asUINT ByteCount = 32;

	static bool TryCreate(TConstArrayView<uint8> Bytes, asSFingerprintDigest& OutDigest);

	bool IsZero() const;
	FString ToHexString() const;
	TConstArrayView<uint8> GetBytes() const { return MakeArrayView(Value); }

	bool operator==(const asSFingerprintDigest& Other) const;
	bool operator!=(const asSFingerprintDigest& Other) const { return !(*this == Other); }

private:
	uint8 Value[ByteCount] = {};
};

struct ANGELSCRIPTRUNTIME_API asSSchemaHash
{
	asSFingerprintDigest Digest;
	bool operator==(const asSSchemaHash& Other) const { return Digest == Other.Digest; }
	bool operator!=(const asSSchemaHash& Other) const { return Digest != Other.Digest; }
};

struct ANGELSCRIPTRUNTIME_API asSLayoutHash
{
	asSFingerprintDigest Digest;
	bool operator==(const asSLayoutHash& Other) const { return Digest == Other.Digest; }
	bool operator!=(const asSLayoutHash& Other) const { return Digest != Other.Digest; }
};

class ANGELSCRIPTRUNTIME_API asIFingerprintDigestProvider
{
public:
	virtual ~asIFingerprintDigestProvider() = default;
	virtual asSFingerprintDigest ComputeDigest(TConstArrayView<uint8> CanonicalDescription) const = 0;
};

ANGELSCRIPTRUNTIME_API asEFingerprintEquality CompareSchemaFingerprints(
	const asSSchemaHash& LeftHash,
	TConstArrayView<uint8> LeftWitness,
	const asSSchemaHash& RightHash,
	TConstArrayView<uint8> RightWitness);

ANGELSCRIPTRUNTIME_API asEFingerprintEquality CompareLayoutFingerprints(
	const asSLayoutHash& LeftHash,
	TConstArrayView<uint8> LeftWitness,
	const asSLayoutHash& RightHash,
	TConstArrayView<uint8> RightWitness);

ANGELSCRIPTRUNTIME_API uint32 GetTypeHash(const asSFingerprintDigest& Digest);

END_AS_NAMESPACE
