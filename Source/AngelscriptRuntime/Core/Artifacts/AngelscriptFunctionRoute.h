#pragma once

#include "Artifacts/AngelscriptArtifactIdentity.h"

class asIScriptFunction;

enum class EAngelscriptFunctionExecutionRoute : uint8
{
	Vm = 1,
	Native = 2,
};

// Shared typed result vocabulary. Cache V2 initially publishes Exact or the
// two identity/availability misses; the provider matcher fills the remaining
// reasons without inventing a competing route representation.
enum class EAngelscriptArtifactMatchResult : uint8
{
	Exact = 0,
	MissingArtifactIdentity = 1,
	MissingProviderEntry = 2,
	ContentMismatch = 3,
	DebugMismatch = 4,
	ProfileMismatch = 5,
	EnvironmentMismatch = 6,
	EntryAbiMismatch = 7,
	MissingReference = 8,
	AmbiguousReference = 9,
	WrongReferenceKind = 10,
	ReferenceAbiMismatch = 11,
	StaleProviderGeneration = 12,
	DuplicateProviderEntry = 13,
	AmbiguousExactProvider = 14,
	ArtifactSetMismatch = 15,
	InvalidProvider = 16,
	MissingEntryPoint = 17,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionRouteGeneration
{
	uint64 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	friend bool operator==(
		const FAngelscriptFunctionRouteGeneration& Left,
		const FAngelscriptFunctionRouteGeneration& Right)
	{
		return Left.Value == Right.Value;
	}

	friend bool operator!=(
		const FAngelscriptFunctionRouteGeneration& Left,
		const FAngelscriptFunctionRouteGeneration& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptFunctionRouteGeneration& Left,
		const FAngelscriptFunctionRouteGeneration& Right)
	{
		return Left.Value < Right.Value;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionRoute
{
	FAngelscriptFunctionRouteGeneration Generation;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptFunctionArtifactIdentity Identity;
	FString CanonicalDeclaration;
	asIScriptFunction* Function = nullptr;
	int32 NumericFunctionId = -1;
	EAngelscriptFunctionExecutionRoute SelectedExecutionRoute =
		EAngelscriptFunctionExecutionRoute::Vm;
	EAngelscriptArtifactMatchResult MatchResult =
		EAngelscriptArtifactMatchResult::MissingProviderEntry;
	bool bHasVerifiedArtifactIdentity = false;

	bool IsNative() const
	{
		return SelectedExecutionRoute
				== EAngelscriptFunctionExecutionRoute::Native
			&& MatchResult == EAngelscriptArtifactMatchResult::Exact;
	}
};

// One immutable Engine-session publication. Live pointers and numeric ids are
// transient; only stable identity values may cross process boundaries.
struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionRouteSnapshot
{
	FAngelscriptFunctionRouteGeneration Generation;
	uint32 VmRouteCount = 0;
	uint32 NativeRouteCount = 0;
	TArray<FAngelscriptFunctionRoute> FunctionRoutes;
};

// Mutable publication owner kept private to one FAngelscriptEngine. Published
// snapshots are exposed only as shared const values.
struct FAngelscriptFunctionRouteState
{
	uint64 NextGenerationValue = 1;
	TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Snapshot;
};
