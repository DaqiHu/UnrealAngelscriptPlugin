#pragma once

#include "Cache/AngelscriptCacheTypes.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheASTBodySidecar
{
	// Must match asAST_SIDECAR_SCHEMA_VERSION in the maintained fork. V6 adds
	// snapshot-owned neutral source-provenance ranges. V5 added the sealed
	// member-to-access-specifier declaration edge. V4 added the
	// sealed direct/virtual/indirect CallExpr dispatch fact. V3 added
	// the sealed class-layout and constant-value facts consumed by Cache V2;
	// accepting the old V2 number here makes the Runtime wrapper reject every
	// valid current DTO after the fork encoder has already verified it.
	static constexpr uint16 SchemaVersion = 6;

	FString FunctionKey;
	FString Profile;
	TArray<uint8> CanonicalAstBytes;
};

enum class EAngelscriptCacheASTBodySidecarError : uint8
{
	Ok = 0,
	EmptyPayloadNotAbsence = 1,
	UnknownNodeOrTypeKind = 2,
	FunctionKeyMismatch = 3,
	ProfileMismatch = 4,
	Budget = 5,
	PointerOrNumericId = 6,
	Malformed = 7,
	OwnerMismatch = 8,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheASTBodySidecarResult
{
	EAngelscriptCacheASTBodySidecarError Error = EAngelscriptCacheASTBodySidecarError::Ok;
	bool IsSuccess() const { return Error == EAngelscriptCacheASTBodySidecarError::Ok; }
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheASTBodySidecarResult AngelscriptEncodeASTBodySidecar(
	const FAngelscriptCacheASTBodySidecar& Sidecar,
	TArray<uint8>& OutBytes);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheASTBodySidecarResult AngelscriptDecodeASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	const FString& ExpectedFunctionKey,
	const FString& ExpectedProfile,
	FAngelscriptCacheASTBodySidecar& OutSidecar);

// Performs bounded Canonical AST envelope/header inspection and returns the durable
// owner/profile without materializing the optional AST graph. Cache admission
// uses this so the default VM-only restore does not pay AST reconstruction
// cost. A caller that wants to retain a snapshot must use the strict decode
// API above with its exact owner and profile.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheASTBodySidecarResult AngelscriptInspectASTBodySidecar(
	const TConstArrayView<uint8> Bytes,
	FAngelscriptCacheASTBodySidecar& OutSidecar);
