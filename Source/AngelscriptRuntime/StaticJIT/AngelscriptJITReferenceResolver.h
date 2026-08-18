#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactReference.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITProvider.h"

struct FAngelscriptEngine;

enum class EAngelscriptJITReferenceResolutionCode : uint8
{
	Exact = 0,
	Missing,
	Ambiguous,
	WrongKind,
	AbiMismatch,
};

/** Opaque ownership token retained by immutable resolved reference tables. */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceLifetime
{
public:
	virtual ~FAngelscriptJITReferenceLifetime() = default;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceResolution
{
	EAngelscriptJITReferenceResolutionCode Code =
		EAngelscriptJITReferenceResolutionCode::Missing;
	const void* Value = nullptr;
	TSharedPtr<const FAngelscriptJITReferenceLifetime,
		ESPMode::ThreadSafe> Lifetime;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptJITReferenceResolver
{
public:
	virtual ~IAngelscriptJITReferenceResolver() = default;

	virtual FAngelscriptJITReferenceResolution Resolve(
		const FAngelscriptArtifactReference& Reference) const = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITResolvedReferenceSlot
{
	uint32 SlotIndex = 0;
	EAngelscriptArtifactReferenceKind Kind =
		EAngelscriptArtifactReferenceKind::Invalid;
	const void* Value = nullptr;
	bool bOptional = false;
	TSharedPtr<const FAngelscriptJITReferenceLifetime,
		ESPMode::ThreadSafe> Lifetime;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITResolvedReferenceTable
{
	TArray<FAngelscriptJITResolvedReferenceSlot> Slots;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceDescriptorBuilder
{
public:
	static FAngelscriptArtifactReference ForFunction(
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptHash256& ExpectedAbi);
	static FAngelscriptArtifactReference ForType(
		const FAngelscriptStableTypeKey& TypeKey,
		const FAngelscriptHash256& ExpectedAbi);
	static FAngelscriptArtifactReference ForGlobal(
		const FAngelscriptStableGlobalKey& GlobalKey,
		const FAngelscriptHash256& ExpectedAbi);
	static FAngelscriptArtifactReference ForImport(
		const FAngelscriptHash256& ImportKey,
		const FAngelscriptHash256& ExpectedAbi);
	/**
	 * Build the Engine-local mutable import-slot descriptor used by generated
	 * code. The stable key identifies the consumer-owned slot; ExpectedAbi
	 * describes only the callable signature and never the currently bound
	 * provider.
	 */
	static FAngelscriptArtifactReference ForImportSlot(
		const FAngelscriptStableModuleKey& ConsumerModuleKey,
		FStringView CanonicalNamespace,
		FStringView CanonicalDeclaration,
		FStringView DeclaredSourceModule);
	static FAngelscriptArtifactReference ForStringLiteral(
		FStringView Value,
		const FAngelscriptHash256& ExpectedAbi);
	/** Build the complete content-defined descriptor used by generated code. */
	static FAngelscriptArtifactReference ForStringLiteral(FStringView Value);
	static FAngelscriptArtifactReference ForRuntimeHelper(
		FStringView CanonicalSymbol,
		const FAngelscriptHash256& ExpectedAbi);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceBinding
{
	FAngelscriptArtifactReference Reference;
	const void* Value = nullptr;
	TSharedPtr<const FAngelscriptJITReferenceLifetime,
		ESPMode::ThreadSafe> Lifetime;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceResolver final
	: public IAngelscriptJITReferenceResolver
{
public:
	FAngelscriptJITReferenceResolver(
		const void* InEngineIdentity,
		TArray<FAngelscriptJITReferenceBinding>&& InBindings);

	FAngelscriptJITReferenceResolution Resolve(
		const FAngelscriptArtifactReference& Reference) const override;

	const void* GetEngineIdentity() const
	{
		return EngineIdentity;
	}

private:
	const void* EngineIdentity = nullptr;
	TArray<FAngelscriptJITReferenceBinding> Bindings;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceIndexBuilder
{
public:
	explicit FAngelscriptJITReferenceIndexBuilder(const void* InEngineIdentity)
		: EngineIdentity(InEngineIdentity)
	{
	}

	bool Add(
		const FAngelscriptArtifactReference& Reference,
		const void* Value);
	bool Add(
		const FAngelscriptArtifactReference& Reference,
		const void* Value,
		TSharedPtr<const FAngelscriptJITReferenceLifetime,
			ESPMode::ThreadSafe> Lifetime);

	/** Add and retain one current-Engine script function until all tables retire. */
	bool AddScriptFunction(
		const FAngelscriptArtifactReference& Reference,
		asIScriptFunction* Function);

	TSharedRef<const FAngelscriptJITReferenceResolver,
		ESPMode::ThreadSafe> Build() const;

private:
	const void* EngineIdentity = nullptr;
	TArray<FAngelscriptJITReferenceBinding> Bindings;
};

enum class EAngelscriptJITCurrentEngineReferenceBuildCode : uint8
{
	Ready = 0,
	InvalidEngine,
	MissingCurrentPublication,
};

struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITCurrentEngineReferenceBuildResult
{
	EAngelscriptJITCurrentEngineReferenceBuildCode Code =
		EAngelscriptJITCurrentEngineReferenceBuildCode::InvalidEngine;
	uint32 RequestedReferenceCount = 0;
	uint32 ResolvedReferenceCount = 0;
	TSharedPtr<const FAngelscriptJITReferenceResolver,
		ESPMode::ThreadSafe> Resolver;
	FString Detail;

	bool IsUsable() const
	{
		return Code == EAngelscriptJITCurrentEngineReferenceBuildCode::Ready
			&& Resolver.IsValid();
	}
};

/**
 * Builds one immutable reference resolver from the current Engine and the
 * current Cache V2 semantic publication. Provider ABI values are never trusted
 * as the current truth: script-symbol ABI comes from the validated publication,
 * while application symbols are rebuilt from the current registration surface.
 */
class ANGELSCRIPTRUNTIME_API
FAngelscriptJITCurrentEngineReferenceResolverBuilder
{
public:
	static FAngelscriptJITCurrentEngineReferenceBuildResult Build(
		FAngelscriptEngine& Engine,
		TConstArrayView<FAngelscriptArtifactReference> RequiredReferences);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceTableBuildResult
{
	EAngelscriptArtifactMatchResult Result =
		EAngelscriptArtifactMatchResult::MissingReference;
	int32 FailedSlotIndex = INDEX_NONE;
	TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
		ESPMode::ThreadSafe> Table;

	bool IsExact() const
	{
		return Result == EAngelscriptArtifactMatchResult::Exact
			&& Table.IsValid();
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITResolvedReferenceTableBuilder
{
public:
	static FAngelscriptJITReferenceTableBuildResult Build(
		TConstArrayView<FAngelscriptJITReferenceSlot> Slots,
		const IAngelscriptJITReferenceResolver& Resolver);
};
