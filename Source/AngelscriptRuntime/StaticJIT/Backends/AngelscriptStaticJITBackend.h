#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneration.h"

#if AS_CAN_GENERATE_JIT

class asIScriptFunction;
class asIScriptModule;
class asITypeInfo;

/**
 * Stable identifier for an internal, build-time StaticJIT backend.
 *
 * This is deliberately a different C++ type and namespace from the exported
 * Runtime JIT BackendId. Static backends are Runtime-module-owned and are not
 * a third-party binary interface.
 */
struct FAngelscriptStaticJITBackendId
{
public:
	FAngelscriptStaticJITBackendId() = default;

	static FAngelscriptStaticJITBackendId Bytecode()
	{
		return FAngelscriptStaticJITBackendId(TEXT("bytecode"));
	}

	static FAngelscriptStaticJITBackendId TypedAST()
	{
		return FAngelscriptStaticJITBackendId(TEXT("typed-ast"));
	}

	static bool TryParse(
		const FStringView Value,
		FAngelscriptStaticJITBackendId& OutBackendId)
	{
		OutBackendId = {};
		if (Value.Equals(TEXT("bytecode"), ESearchCase::CaseSensitive))
		{
			OutBackendId = Bytecode();
			return true;
		}
		if (Value.Equals(TEXT("typed-ast"), ESearchCase::CaseSensitive))
		{
			OutBackendId = TypedAST();
			return true;
		}
		return false;
	}

	bool IsValid() const
	{
		return Value == TEXT("bytecode") || Value == TEXT("typed-ast");
	}

	const FString& ToString() const
	{
		return Value;
	}

	friend bool operator==(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return Left.Value == Right.Value;
	}

	friend bool operator!=(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return Left.Value < Right.Value;
	}

private:
	explicit FAngelscriptStaticJITBackendId(const TCHAR* InValue)
		: Value(InValue)
	{
	}

	FString Value;
};

/** One function visible to a synchronous Static generation task. */
struct FAngelscriptStaticJITCompiledFunctionView
{
	asIScriptFunction* EngineLocalFunction = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 ExecutionHash;
	FAngelscriptHash256 DebugHash;
	FAngelscriptHash256 EntryAbiHash;
	FStringView CanonicalDeclaration;
	TConstArrayView<uint8> Bytecode;
	TConstArrayView<uint8> VerifiedTypedHIR;
	bool bHasVerifiedArtifactIdentity = false;
	bool bHasVerifiedArtifactReferences = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
};

/** One type visible to task-wide analysis. EngineLocalType dies with the task. */
struct FAngelscriptStaticJITCompiledTypeView
{
	asITypeInfo* EngineLocalType = nullptr;
	FAngelscriptHash256 StableTypeKey;
	FStringView CanonicalDeclaration;
	bool bTargetMaterializesScriptReflection = false;
};

/** One global visible to task-wide analysis. */
struct FAngelscriptStaticJITCompiledGlobalView
{
	const void* EngineLocalGlobal = nullptr;
	FAngelscriptHash256 StableGlobalKey;
	FStringView CanonicalDeclaration;
};

enum class EAngelscriptStaticJITDescriptorKind : uint8
{
	Class = 0,
	Struct,
	Enum,
	Delegate,
	Property,
	Function,
};

/** Pointer-free descriptor facts plus an optional synchronous local handle. */
struct FAngelscriptStaticJITDescriptorView
{
	const void* EngineLocalDescriptor = nullptr;
	FAngelscriptHash256 StableDescriptorKey;
	FAngelscriptHash256 EntryPlanHash;
	FStringView CanonicalDeclaration;
	EAngelscriptStaticJITDescriptorKind Kind =
		EAngelscriptStaticJITDescriptorKind::Class;
	FStringView CanonicalName;
	FStringView ReceiverCanonicalName;
	bool bUFunctionRoot = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
};

/** Stable dependency or external native-call fact used by a backend. */
struct FAngelscriptStaticJITDependencyView
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	FAngelscriptArtifactReference Reference;
	FStringView CanonicalDeclaration;
};

/**
 * Complete Provider-domain graph available to a backend before any function
 * is emitted. Engine-local pointers are synchronous views only; backend
 * output must contain stable, pointer-free values.
 */
struct FAngelscriptStaticJITCompiledSourceGraph
{
	FAngelscriptJITProviderGenerationSettings Settings;
	TConstArrayView<asIScriptModule*> Modules;
	TConstArrayView<FAngelscriptStaticJITCompiledFunctionView> Functions;
	TConstArrayView<FAngelscriptStaticJITCompiledTypeView> Types;
	TConstArrayView<FAngelscriptStaticJITCompiledGlobalView> Globals;
	TConstArrayView<FAngelscriptStaticJITDescriptorView> Descriptors;
	TConstArrayView<FAngelscriptStaticJITDependencyView> Dependencies;
	TConstArrayView<FAngelscriptStaticJITDependencyView> ExternalNativeCalls;
	bool bHasVerifiedTypedHIR = false;
};

/** Immutable task-wide input; EmitModuleSet restricts output, not analysis. */
struct FAngelscriptStaticJITBackendRequest
{
	FAngelscriptStaticJITBackendId RequestedBackendId;
	const FAngelscriptStaticJITCompiledSourceGraph& CompiledSourceGraph;
	TConstArrayView<FAngelscriptStableModuleKey> EmitModuleSet;
};

enum class EAngelscriptStaticJITFunctionDisposition : uint8
{
	Emitted = 0,
	Unsupported,
	Failed,
};

/** Pointer-free result for one stable function identity. */
struct FAngelscriptStaticJITFunctionResult
{
	EAngelscriptStaticJITFunctionDisposition Disposition =
		EAngelscriptStaticJITFunctionDisposition::Unsupported;
	FAngelscriptStaticJITBackendId ActualBackendId;
	FAngelscriptStableFunctionKey FunctionKey;
	FString Detail;
	TOptional<FAngelscriptJITGenerationFunction> EmittedFunction;
};

enum class EAngelscriptStaticJITTaskDisposition : uint8
{
	Succeeded = 0,
	FatalError,
};

/**
 * Backend-neutral task result consumed by the generator and deterministic
 * Provider packager. Provenance is diagnostic metadata, never Provider ID.
 */
struct FAngelscriptStaticJITBackendOutput
{
	EAngelscriptStaticJITTaskDisposition Disposition =
		EAngelscriptStaticJITTaskDisposition::Succeeded;
	FString Detail;
	TArray<FAngelscriptJITGenerationModule> Modules;
	TArray<FAngelscriptStaticJITFunctionResult> Functions;
	FString ModuleSourcePreamble;
	FString ModuleSourceFooter;
	FString ProviderSourcePreamble;
};

/** One synchronous backend instance created for exactly one generation task. */
class IAngelscriptStaticJITBackend
{
public:
	virtual ~IAngelscriptStaticJITBackend() = default;

	virtual bool Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITBackendOutput& OutOutput,
		FString* OutError) = 0;
};

/** Internal factory; explicit module registration avoids constructor order. */
class IAngelscriptStaticJITBackendFactory
{
public:
	virtual ~IAngelscriptStaticJITBackendFactory() = default;

	virtual FAngelscriptStaticJITBackendId GetBackendId() const = 0;
	virtual TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const = 0;
};

enum class EAngelscriptStaticJITBackendRegistrationCode : uint8
{
	Registered = 0,
	InvalidBackendId,
	DuplicateBackendId,
};

struct FAngelscriptStaticJITBackendRegistrationResult
{
	EAngelscriptStaticJITBackendRegistrationCode Code =
		EAngelscriptStaticJITBackendRegistrationCode::InvalidBackendId;
	FAngelscriptStaticJITBackendId BackendId;

	bool IsAccepted() const
	{
		return Code ==
			EAngelscriptStaticJITBackendRegistrationCode::Registered;
	}
};

/** Small, task-orchestrator-owned registry with deterministic ID lookup. */
class FAngelscriptStaticJITBackendRegistry
{
public:
	FAngelscriptStaticJITBackendRegistrationResult RegisterFactory(
		const IAngelscriptStaticJITBackendFactory& Factory)
	{
		const FAngelscriptStaticJITBackendId BackendId =
			Factory.GetBackendId();
		if (!BackendId.IsValid())
		{
			return {
				EAngelscriptStaticJITBackendRegistrationCode::InvalidBackendId,
				BackendId};
		}
		if (FindFactory(BackendId) != nullptr)
		{
			return {
				EAngelscriptStaticJITBackendRegistrationCode::DuplicateBackendId,
				BackendId};
		}

		Factories.Add(&Factory);
		Factories.Sort([](
			const IAngelscriptStaticJITBackendFactory& Left,
			const IAngelscriptStaticJITBackendFactory& Right)
		{
			return Left.GetBackendId() < Right.GetBackendId();
		});
		return {
			EAngelscriptStaticJITBackendRegistrationCode::Registered,
			BackendId};
	}

	const IAngelscriptStaticJITBackendFactory* FindFactory(
		const FAngelscriptStaticJITBackendId& BackendId) const
	{
		if (!BackendId.IsValid())
		{
			return nullptr;
		}
		const IAngelscriptStaticJITBackendFactory* const* Found =
			Factories.FindByPredicate(
				[&BackendId](
					const IAngelscriptStaticJITBackendFactory* Candidate)
				{
					return Candidate != nullptr
						&& Candidate->GetBackendId() == BackendId;
				});
		return Found != nullptr ? *Found : nullptr;
	}

	TArray<FAngelscriptStaticJITBackendId> GetRegisteredBackendIds() const
	{
		TArray<FAngelscriptStaticJITBackendId> Result;
		Result.Reserve(Factories.Num());
		for (const IAngelscriptStaticJITBackendFactory* Factory : Factories)
		{
			if (Factory != nullptr)
			{
				Result.Add(Factory->GetBackendId());
			}
		}
		return Result;
	}

private:
	TArray<const IAngelscriptStaticJITBackendFactory*> Factories;
};

#endif
