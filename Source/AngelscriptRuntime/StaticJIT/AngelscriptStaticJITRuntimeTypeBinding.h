#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

class asITypeInfo;

/** Exact function-signature coordinate authenticated by the Canonical generation. */
enum class EAngelscriptStaticJITRuntimeTypeBindingRole : uint8
{
	Invalid = 0,
	Return,
	Parameter,
	Receiver,
};

/**
 * Owned, pointer-free copy of one maintained-fork Runtime ABI key.
 *
 * LayoutHash is an accelerator/checksum only. Complete equality is defined by
 * the stable identity and the full profile/environment/layout/behaviour facts.
 */
struct FAngelscriptStaticJITRuntimeTypeABIKey
{
	FString StableTypeKey;
	FString TargetProfileKey;
	FString NativeEnvironmentKey;
	uint8 TypeKind = 0;
	uint32 Qualifiers = 0;
	uint64 ObjectFlags = 0;
	int32 ByteSize = INDEX_NONE;
	int32 ByteAlignment = INDEX_NONE;
	FString LayoutSignature;
	FString BehaviourSignature;
	uint64 LayoutHash = 0;

	bool IsStructurallyComplete() const
	{
		return !StableTypeKey.IsEmpty()
			&& !TargetProfileKey.IsEmpty()
			&& !NativeEnvironmentKey.IsEmpty()
			&& TypeKind != 0
			&& ByteSize >= 0
			&& ByteAlignment > 0;
	}

	bool IsExactlyCompatibleWith(
		const FAngelscriptStaticJITRuntimeTypeABIKey& Other) const
	{
		return StableTypeKey == Other.StableTypeKey
			&& TargetProfileKey == Other.TargetProfileKey
			&& NativeEnvironmentKey == Other.NativeEnvironmentKey
			&& TypeKind == Other.TypeKind
			&& Qualifiers == Other.Qualifiers
			&& ObjectFlags == Other.ObjectFlags
			&& ByteSize == Other.ByteSize
			&& ByteAlignment == Other.ByteAlignment
			&& LayoutSignature == Other.LayoutSignature
			&& BehaviourSignature == Other.BehaviourSignature;
	}
};

/**
 * Pointer-free authentication result that may be copied into generated output.
 * It contains no AST/snapshot ID, Engine pointer, numeric TypeId, or live table
 * address.
 */
struct FAngelscriptStaticJITRuntimeTypeBindingSummary
{
	FAngelscriptStableModuleKey ModuleKey;
	FString OwnerCanonicalFunctionDeclKey;
	EAngelscriptStaticJITRuntimeTypeBindingRole Role =
		EAngelscriptStaticJITRuntimeTypeBindingRole::Invalid;
	uint32 FormalIndex = MAX_uint32;
	FAngelscriptStaticJITRuntimeTypeABIKey ExpectedABI;
	FAngelscriptStaticJITRuntimeTypeABIKey ResolvedABI;

	bool IsStructurallyComplete() const
	{
		const bool bCoordinateComplete =
			Role == EAngelscriptStaticJITRuntimeTypeBindingRole::Parameter
				? FormalIndex != MAX_uint32
				: FormalIndex == MAX_uint32;
		return !ModuleKey.Hash.IsZero()
			&& !OwnerCanonicalFunctionDeclKey.IsEmpty()
			&& Role != EAngelscriptStaticJITRuntimeTypeBindingRole::Invalid
			&& bCoordinateComplete
			&& ExpectedABI.IsStructurallyComplete()
			&& ResolvedABI.IsStructurallyComplete();
	}
};

/**
 * Generation/request-owned synchronous view. EngineLocalType is only a
 * same-generation coordinate used to authenticate ownership and must never be
 * copied into Provider output.
 */
struct FAngelscriptStaticJITRuntimeTypeBinding
{
	FAngelscriptStaticJITRuntimeTypeBindingSummary Summary;
	asITypeInfo* EngineLocalType = nullptr;

	bool IsStructurallyComplete() const
	{
		return Summary.IsStructurallyComplete()
			&& EngineLocalType != nullptr;
	}
};

#endif
