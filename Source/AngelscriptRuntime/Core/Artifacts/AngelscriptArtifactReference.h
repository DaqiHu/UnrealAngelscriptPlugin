#pragma once

#include "Artifacts/AngelscriptArtifactIdentity.h"

// Stable reference vocabulary shared by persisted Cache artifacts and
// process-local StaticJIT provider manifests. Numeric ids and live pointers are
// deliberately excluded from this value.
enum class EAngelscriptArtifactReferenceKind : uint8
{
	Invalid = 0,
	ScriptModule = 1,
	ScriptType = 2,
	ScriptFunction = 3,
	ScriptGlobal = 4,
	ScriptProperty = 5,
	ScriptImport = 6,
	EnvironmentSymbol = 7,
	CanonicalName = 8,
	StringLiteral = 9,
	RuntimeHelper = 10,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptArtifactReference
{
	EAngelscriptArtifactReferenceKind Kind =
		EAngelscriptArtifactReferenceKind::Invalid;
	FAngelscriptHash256 StableKey;
	FAngelscriptHash256 ExpectedAbi;

	bool IsValid() const
	{
		return Kind != EAngelscriptArtifactReferenceKind::Invalid
			&& !StableKey.IsZero();
	}

	friend bool operator==(
		const FAngelscriptArtifactReference& Left,
		const FAngelscriptArtifactReference& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.StableKey == Right.StableKey
			&& Left.ExpectedAbi == Right.ExpectedAbi;
	}

	friend bool operator!=(
		const FAngelscriptArtifactReference& Left,
		const FAngelscriptArtifactReference& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptArtifactReference& Left,
		const FAngelscriptArtifactReference& Right)
	{
		const uint8 LeftKind = static_cast<uint8>(Left.Kind);
		const uint8 RightKind = static_cast<uint8>(Right.Kind);
		if (LeftKind != RightKind)
		{
			return LeftKind < RightKind;
		}
		if (!(Left.StableKey == Right.StableKey))
		{
			return Left.StableKey < Right.StableKey;
		}
		return Left.ExpectedAbi < Right.ExpectedAbi;
	}
};

/**
 * Pointer-free, storage-neutral references required by one verified function
 * artifact. Cache capture publishes this value for Runtime consumers such as
 * StaticJIT generation; consumers never need to decode Cache records or retain
 * VM object pointers merely to describe a Provider entry.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionArtifactReferenceSet
{
	FAngelscriptStableFunctionKey FunctionKey;
	TArray<FAngelscriptArtifactReference> RequiredReferences;
};
