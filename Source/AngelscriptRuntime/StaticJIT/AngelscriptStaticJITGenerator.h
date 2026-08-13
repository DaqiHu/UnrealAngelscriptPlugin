#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptStaticJITGenerationSnapshot;

/** Provider artifacts plus pointer-free per-function backend diagnostics. */
struct FAngelscriptStaticJITGeneratorOutput
{
	bool bHasProviderOutput = false;
	FAngelscriptJITGenerationOutput ProviderOutput;
	TArray<FAngelscriptStaticJITFunctionResult> Functions;
};

/**
 * Internal backend-neutral StaticJIT task orchestrator.
 *
 * The registry and its factories outlive this generator. A backend instance
 * is created at most once for each backend participating in one Generate()
 * call, receives the complete graph once, and is destroyed before Generate()
 * returns.
 */
class FAngelscriptStaticJITGenerator
{
public:
	explicit FAngelscriptStaticJITGenerator(
		const FAngelscriptStaticJITBackendRegistry& InRegistry)
		: Registry(InRegistry)
	{
	}

	bool Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITGeneratorOutput& OutOutput,
		FString* OutError = nullptr) const
	{
		OutOutput = {};
		auto Fail = [OutError](const FString& Detail)
		{
			if (OutError != nullptr)
			{
				*OutError = Detail;
			}
			return false;
		};

		if (!Request.RequestedBackendId.IsValid())
		{
			return Fail(TEXT(
				"StaticJIT generation requires a valid Static BackendId."));
		}
		if (Request.RequestedBackendId
				== FAngelscriptStaticJITBackendId::TypedAST()
			&& !Request.CompiledSourceGraph.bHasVerifiedTypedHIR)
		{
			return Fail(TEXT(
				"StaticJIT generation failed: CaptureProfileMismatch for typed-ast."));
		}

		TArray<FAngelscriptStaticJITBackendId> BackendChain;
		BackendChain.Add(Request.RequestedBackendId);
		if (Request.RequestedBackendId
			== FAngelscriptStaticJITBackendId::TypedAST())
		{
			BackendChain.Add(FAngelscriptStaticJITBackendId::Bytecode());
		}

		OutOutput.Functions.Reserve(
			Request.CompiledSourceGraph.Functions.Num());
		for (const FAngelscriptStaticJITCompiledFunctionView& Function :
			Request.CompiledSourceGraph.Functions)
		{
			FAngelscriptStaticJITFunctionResult& Result =
				OutOutput.Functions.Emplace_GetRef();
			Result.FunctionKey = Function.FunctionKey;
		}

		TArray<FAngelscriptJITGenerationModule> GeneratedModules;
		FString ModuleSourcePreamble;
		FString ModuleSourceFooter;
		FString ProviderSourcePreamble;
		for (int32 BackendIndex = 0;
			BackendIndex < BackendChain.Num();
			++BackendIndex)
		{
			const FAngelscriptStaticJITBackendId BackendId =
				BackendChain[BackendIndex];
			const IAngelscriptStaticJITBackendFactory* const Factory =
				Registry.FindFactory(BackendId);
			if (Factory == nullptr)
			{
				if (BackendIndex == 0)
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backend '%s' is not registered."),
						*BackendId.ToString()));
				}
				continue;
			}

			TUniquePtr<IAngelscriptStaticJITBackend> Backend =
				Factory->CreateBackend();
			if (!Backend.IsValid())
			{
				return Fail(FString::Printf(TEXT(
					"StaticJIT backend factory '%s' returned no task instance."),
					*BackendId.ToString()));
			}

			FAngelscriptStaticJITBackendOutput BackendOutput;
			FString BackendError;
			const FAngelscriptStaticJITBackendRequest ActiveBackendRequest{
				BackendId,
				Request.CompiledSourceGraph,
				Request.EmitModuleSet};
			if (!Backend->Generate(
				ActiveBackendRequest, BackendOutput, &BackendError))
			{
				return Fail(BackendError.IsEmpty()
					? FString::Printf(TEXT(
						"StaticJIT backend '%s' failed its generation task."),
						*BackendId.ToString())
					: BackendError);
			}
			if (BackendOutput.Disposition
				== EAngelscriptStaticJITTaskDisposition::FatalError)
			{
				return Fail(BackendOutput.Detail.IsEmpty()
					? FString::Printf(TEXT(
						"StaticJIT backend '%s' reported a fatal task error."),
						*BackendId.ToString())
					: BackendOutput.Detail);
			}

			for (const FAngelscriptJITGenerationModule& Module :
				BackendOutput.Modules)
			{
				FAngelscriptJITGenerationModule* const Existing =
					GeneratedModules.FindByPredicate(
						[&Module](
							const FAngelscriptJITGenerationModule& Candidate)
						{
							return Candidate.ModuleKey == Module.ModuleKey;
						});
				if (Existing == nullptr)
				{
					GeneratedModules.Add(Module);
				}
				else if (Existing->CanonicalModuleName
						!= Module.CanonicalModuleName
					|| Existing->VirtualSourcePath != Module.VirtualSourcePath)
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backends disagreed on module %s."),
						*Module.ModuleKey.Hash.ToHexString()));
				}
			}

			AppendFragment(ModuleSourcePreamble,
				BackendOutput.ModuleSourcePreamble);
			AppendFragment(ModuleSourceFooter,
				BackendOutput.ModuleSourceFooter);
			AppendFragment(ProviderSourcePreamble,
				BackendOutput.ProviderSourcePreamble);

			TArray<FAngelscriptStableFunctionKey> BackendFunctionKeys;
			for (FAngelscriptStaticJITFunctionResult& Candidate :
				BackendOutput.Functions)
			{
				if (Candidate.FunctionKey.Hash.IsZero())
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backend '%s' returned a function with no stable key."),
						*BackendId.ToString()));
				}
				if (BackendFunctionKeys.Contains(Candidate.FunctionKey))
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backend '%s' returned duplicate function %s."),
						*BackendId.ToString(),
						*Candidate.FunctionKey.Hash.ToHexString()));
				}
				BackendFunctionKeys.Add(Candidate.FunctionKey);
				if (!Candidate.ActualBackendId.IsValid()
					|| Candidate.ActualBackendId != BackendId)
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backend '%s' returned invalid provenance."),
						*BackendId.ToString()));
				}

				FAngelscriptStaticJITFunctionResult* Result =
					OutOutput.Functions.FindByPredicate(
						[&Candidate](
							const FAngelscriptStaticJITFunctionResult& Existing)
						{
							return Existing.FunctionKey
								== Candidate.FunctionKey;
						});
				if (Result == nullptr
					&& !Request.CompiledSourceGraph.Functions.IsEmpty())
				{
					return Fail(FString::Printf(TEXT(
						"StaticJIT backend '%s' returned function %s outside the compiled source graph."),
						*BackendId.ToString(),
						*Candidate.FunctionKey.Hash.ToHexString()));
				}
				if (Result == nullptr)
				{
					Result = &OutOutput.Functions.Emplace_GetRef();
					Result->FunctionKey = Candidate.FunctionKey;
				}
				if (Result->Disposition
					== EAngelscriptStaticJITFunctionDisposition::Emitted)
				{
					continue;
				}
				if (Candidate.Disposition
					== EAngelscriptStaticJITFunctionDisposition::Emitted)
				{
					if (!Candidate.EmittedFunction.IsSet()
						|| Candidate.EmittedFunction->FunctionKey
							!= Candidate.FunctionKey)
					{
						return Fail(FString::Printf(TEXT(
							"StaticJIT backend '%s' returned an invalid emitted function."),
							*BackendId.ToString()));
					}
					*Result = MoveTemp(Candidate);
				}
				else
				{
					*Result = MoveTemp(Candidate);
				}
			}
		}

		return PackageBackendResults(
			Request,
			GeneratedModules,
			OutOutput.Functions,
			MoveTemp(ModuleSourcePreamble),
			MoveTemp(ModuleSourceFooter),
			MoveTemp(ProviderSourcePreamble),
			OutOutput,
			OutError);
	}

	static bool PackageBackendResults(
		const FAngelscriptStaticJITBackendRequest& Request,
		const TConstArrayView<FAngelscriptJITGenerationModule> GeneratedModules,
		const TConstArrayView<FAngelscriptStaticJITFunctionResult> FunctionResults,
		FString ModuleSourcePreamble,
		FString ModuleSourceFooter,
		FString ProviderSourcePreamble,
		FAngelscriptStaticJITGeneratorOutput& OutOutput,
		FString* OutError = nullptr)
	{
		OutOutput.bHasProviderOutput = false;
		OutOutput.ProviderOutput = {};
		auto Fail = [OutError](const FString& Detail)
		{
			if (OutError != nullptr)
			{
				*OutError = Detail;
			}
			return false;
		};

		FAngelscriptJITGenerationRequest PackageRequest;
		const FAngelscriptJITProviderGenerationSettings& Settings =
			Request.CompiledSourceGraph.Settings;
		PackageRequest.TargetProfile = Settings.TargetProfile;
		PackageRequest.ProviderId = Settings.ProviderId;
		PackageRequest.ProviderName = Settings.ProviderName;
		PackageRequest.OwnerModuleName = Settings.OwnerModuleName;
		PackageRequest.ArtifactProfile = Settings.ArtifactProfile;
		PackageRequest.NativeEnvironmentFingerprint =
			Settings.NativeEnvironmentFingerprint;
		PackageRequest.ModuleSourcePreamble = MoveTemp(ModuleSourcePreamble);
		PackageRequest.ModuleSourceFooter = MoveTemp(ModuleSourceFooter);
		PackageRequest.ProviderSourcePreamble = MoveTemp(ProviderSourcePreamble);

		auto ShouldEmitModule = [&Request](
			const FAngelscriptStableModuleKey& ModuleKey)
		{
			return Request.EmitModuleSet.ContainsByPredicate(
				[&ModuleKey](const FAngelscriptStableModuleKey& Candidate)
				{
					return Candidate == ModuleKey;
				});
		};
		for (const FAngelscriptJITGenerationModule& Module : GeneratedModules)
		{
			if (ShouldEmitModule(Module.ModuleKey))
			{
				PackageRequest.Modules.Add(Module);
			}
		}
		for (const FAngelscriptStaticJITFunctionResult& Result : FunctionResults)
		{
			if (Result.Disposition
					== EAngelscriptStaticJITFunctionDisposition::Emitted
				&& Result.EmittedFunction.IsSet()
				&& ShouldEmitModule(Result.EmittedFunction->ModuleKey))
			{
				PackageRequest.Functions.Add(*Result.EmittedFunction);
			}
		}

		if (PackageRequest.Functions.IsEmpty())
		{
			return true;
		}
		if (PackageRequest.Modules.IsEmpty())
		{
			return Fail(TEXT(
				"StaticJIT backends emitted functions without an emitted module."));
		}
		if (!FAngelscriptJITGeneration::Generate(
			PackageRequest, OutOutput.ProviderOutput, OutError))
		{
			return false;
		}
		OutOutput.bHasProviderOutput = true;
		return true;
	}

private:
	static void AppendFragment(FString& Target, const FString& Fragment)
	{
		if (Fragment.IsEmpty())
		{
			return;
		}
		FString Normalized = Fragment;
		if (!Normalized.EndsWith(TEXT("\n")))
		{
			Normalized += TEXT("\n");
		}
		if (!Target.Contains(Normalized, ESearchCase::CaseSensitive))
		{
			Target += Normalized;
		}
	}

	const FAngelscriptStaticJITBackendRegistry& Registry;
};

/**
 * Synchronous explicit generation request for Editor/test orchestration.
 * CompiledSourceGraph contains the complete Provider source domain. An empty
 * EmitModuleSet means that complete graph is emitted for compatibility.
 */
struct FAngelscriptStaticJITProviderGenerationRequest
{
	FAngelscriptStaticJITBackendId BackendId =
		FAngelscriptStaticJITBackendId::Bytecode();
	FAngelscriptJITProviderGenerationSettings Settings;
	TConstArrayView<asIScriptModule*> CompiledSourceGraph;
	TConstArrayView<asIScriptModule*> EmitModuleSet;
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> GenerationSnapshot;
	bool bHasVerifiedTypedHIR = false;
};

/** Runtime-module-owned registry. Static backends are registered explicitly. */
ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITBackendRegistry&
GetAngelscriptStaticJITBackendRegistry();

/** Idempotently register the built-in bytecode backend. */
ANGELSCRIPTRUNTIME_API bool RegisterAngelscriptBytecodeJITBackend(
	FString* OutError = nullptr);

/** Explicit BackendId/capture generation entry point for programmatic callers. */
ANGELSCRIPTRUNTIME_API bool GenerateStaticJITProviderArtifacts(
	const FAngelscriptStaticJITProviderGenerationRequest& Request,
	FAngelscriptStaticJITGeneratorOutput& OutOutput,
	FString* OutError = nullptr);

#endif
