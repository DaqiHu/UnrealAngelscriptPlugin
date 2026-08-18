#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

class asCScriptEngine;
class asCScriptFunction;
class asCObjectProperty;
class asCTypeInfo;
class UClass;

// Canonical identity for application-registered AngelScript types. The
// persisted reference contains only canonical AngelScript surface data and ABI
// hashes; current Engine pointers and numeric TypeIds never cross this boundary.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEnvironmentIdentity final
{
public:
	// Enumerates only the functions that are reachable from the Engine's
	// current application-registration surface. scriptFunctions is an ID/history
	// table and can retain removed config-group functions while external
	// references are alive, so environment scans must not use it as a registry.
	static void CollectCurrentRegisteredFunctions(
		const asCScriptEngine& Engine,
		TArray<asCScriptFunction*>& OutFunctions);

	static bool TryBuildTypeReference(
		const asCTypeInfo& Type,
		FAngelscriptArtifactReference& OutReference);

	static bool TryBuildFunctionReference(
		const asCScriptFunction& Function,
		FAngelscriptArtifactReference& OutReference);

	static bool TryBuildPropertyReference(
		const asCTypeInfo& OwnerType,
		const asCObjectProperty& Property,
		FAngelscriptArtifactReference& OutReference);

	static bool TryBuildCodeRootReference(
		const UClass& CodeRoot,
		FAngelscriptArtifactReference& OutReference);
};

// Resolves a persisted EnvironmentSymbol against the application-registered
// types of one current AngelScript Engine. Ambiguous identities fail closed.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEngineEnvironmentResolver final
	: public IAngelscriptCacheCurrentSymbolResolver
{
public:
	explicit FAngelscriptCacheEngineEnvironmentResolver(
		const asCScriptEngine& InEngine);

	virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
		EAngelscriptArtifactReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const override;

	// Read-only diagnostics for cache capture profiling and regression tests.
	// The environment surface is indexed exactly once per resolver session;
	// Resolve() never rebuilds or mutates this snapshot.
	uint32 GetSymbolIndexBuildCountForDiagnostics() const;
	int32 GetIndexedSymbolCountForDiagnostics() const;

	// Materialization-time companions to Resolve(). These return current-Engine
	// objects only after both the stable key and expected ABI match exactly.
	// Ambiguous references fail closed just like the validation-only resolver.
	asCTypeInfo* ResolveTypeReference(
		const FAngelscriptArtifactReference& Expected) const;
	asCScriptFunction* ResolveFunctionReference(
		const FAngelscriptArtifactReference& Expected) const;
	asCObjectProperty* ResolvePropertyReference(
		const FAngelscriptArtifactReference& Expected,
		asCTypeInfo** OutOwnerType = nullptr) const;
	UClass* ResolveCodeRootReference(
		const FAngelscriptArtifactReference& Expected) const;

private:
	struct FIndexedSymbol
	{
		FAngelscriptHash256 StableKey;
		FAngelscriptCacheCurrentSymbol Current;
		bool bAmbiguous = false;
	};

	void BuildSymbolIndexLocked() const;

	const asCScriptEngine* Engine = nullptr;
	mutable FCriticalSection SymbolIndexMutex;
	mutable TArray<FIndexedSymbol> SymbolIndex;
	mutable uint32 SymbolIndexBuildCount = 0;
};

// Resolves the storage facts that are external to a selected script module:
// registered AngelScript value/handle layouts and the reflected C++ code-root
// boundary used by UE-derived script classes.
class ANGELSCRIPTRUNTIME_API FAngelscriptCacheEngineLayoutResolver final
	: public IAngelscriptCacheCurrentLayoutResolver
{
public:
	explicit FAngelscriptCacheEngineLayoutResolver(
		const asCScriptEngine& InEngine);

	virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout>
	ResolveDataTypeLayout(
		const FAngelscriptCachedDataType& DataType,
		const IAngelscriptCacheProspectiveTypeLayoutView& LocalLayouts)
		const override;

	virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
	ResolveTypeLayoutInput(
		EAngelscriptCachedTypeLayoutInputKind InputKind,
		EAngelscriptArtifactReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const override;

private:
	const asCScriptEngine* Engine = nullptr;
};
