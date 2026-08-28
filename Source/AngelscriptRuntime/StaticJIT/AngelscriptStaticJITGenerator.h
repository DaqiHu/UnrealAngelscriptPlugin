#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptStaticJITGenerationSnapshot;

/** Pointer-free audit of the exact immutable graph delivered to a backend. */
struct FAngelscriptStaticJITCompiledSourceGraphSummary
{
	uint32 SourceCompilePassCount = 0;
	int32 ModuleCount = 0;
	int32 FunctionCount = 0;
	int32 TypeCount = 0;
	int32 GlobalCount = 0;
	int32 DescriptorCount = 0;
	int32 DependencyCount = 0;
	int32 ExternalNativeCallCount = 0;
	int32 EmitModuleCount = 0;
	bool bHasVerifiedCanonicalAST = false;
};

struct FAngelscriptStaticJITGeneratorDiagnostics
{
	FAngelscriptStaticJITBackendId RequestedBackendId;
	EAngelscriptStaticJITCaptureProfile CaptureProfile =
		EAngelscriptStaticJITCaptureProfile::Bytecode;
	TArray<FAngelscriptStaticJITBackendId> FallbackChain;
	FAngelscriptStaticJITCompiledSourceGraphSummary CompiledSourceGraph;
	FString TaskError;
};

/** Provider artifacts plus pointer-free per-function backend diagnostics. */
struct FAngelscriptStaticJITGeneratorOutput
{
	bool bHasProviderOutput = false;
	FAngelscriptJITGenerationOutput ProviderOutput;
	FAngelscriptStaticJITGeneratorDiagnostics Diagnostics;
	TArray<FAngelscriptStaticJITFunctionResult> Functions;
};

/** Convert generator-owned backend facts into the Provider packaging model. */
ANGELSCRIPTRUNTIME_API FAngelscriptJITGenerationDiagnostics
BuildAngelscriptStaticJITGenerationDiagnostics(
	const FAngelscriptStaticJITGeneratorOutput& GeneratorOutput,
	TConstArrayView<FAngelscriptStableFunctionKey> EmittedFunctionKeys);

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
		OutOutput.Diagnostics.RequestedBackendId = Request.RequestedBackendId;
		OutOutput.Diagnostics.CaptureProfile =
			Request.CompiledSourceGraph.CaptureProfile;
		FAngelscriptStaticJITCompiledSourceGraphSummary& GraphSummary =
			OutOutput.Diagnostics.CompiledSourceGraph;
		GraphSummary.SourceCompilePassCount =
			Request.CompiledSourceGraph.SourceCompilePassCount;
		GraphSummary.ModuleCount = Request.CompiledSourceGraph.Modules.Num();
		GraphSummary.FunctionCount = Request.CompiledSourceGraph.Functions.Num();
		GraphSummary.TypeCount = Request.CompiledSourceGraph.Types.Num();
		GraphSummary.GlobalCount = Request.CompiledSourceGraph.Globals.Num();
		GraphSummary.DescriptorCount =
			Request.CompiledSourceGraph.Descriptors.Num();
		GraphSummary.DependencyCount =
			Request.CompiledSourceGraph.Dependencies.Num();
		GraphSummary.ExternalNativeCallCount =
			Request.CompiledSourceGraph.ExternalNativeCalls.Num();
		GraphSummary.EmitModuleCount = Request.EmitModuleSet.Num();
		GraphSummary.bHasVerifiedCanonicalAST =
			Request.CompiledSourceGraph.CaptureProfile
				== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
		OutOutput.Diagnostics.FallbackChain.Add(Request.RequestedBackendId);
		if (Request.RequestedBackendId
			== FAngelscriptStaticJITBackendId::TypedAST())
		{
			OutOutput.Diagnostics.FallbackChain.Add(
				FAngelscriptStaticJITBackendId::Bytecode());
		}
		auto Fail = [&OutOutput, OutError](const FString& Detail)
		{
			OutOutput.Diagnostics.TaskError = Detail;
			if (OutError != nullptr)
			{
				*OutError = Detail;
			}
			return false;
		};

		FAngelscriptStaticJITGenerationProfile RequiredProfile;
		FString ProfileError;
		if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
			Request.RequestedBackendId,
			Request.CompiledSourceGraph.Settings.TargetProfile,
			RequiredProfile, &ProfileError))
		{
			return Fail(ProfileError);
		}
		if (Request.CompiledSourceGraph.CaptureProfile
			!= RequiredProfile.CaptureProfile)
		{
			const TCHAR* const RequiredCaptureProfile =
				RequiredProfile.RequiresCanonicalASTSnapshot()
					? TEXT("verified-canonical-ast")
					: TEXT("bytecode");
			const TCHAR* const ActualCaptureProfile =
				Request.CompiledSourceGraph.CaptureProfile
					== EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
					? TEXT("verified-canonical-ast")
					: TEXT("bytecode");
			return Fail(FString::Printf(TEXT(
				"StaticJIT generation failed: CaptureProfileMismatch; backend '%s' requires '%s' but the compiled graph uses '%s'."),
				*Request.RequestedBackendId.ToString(),
				RequiredCaptureProfile,
				ActualCaptureProfile));
		}

		const TArray<FAngelscriptStaticJITBackendId>& BackendChain =
			OutOutput.Diagnostics.FallbackChain;

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
				TArray<FAngelscriptStaticJITBackendAttemptDiagnostic>
					BackendAttempts = MoveTemp(Result->BackendAttempts);
				FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt =
					BackendAttempts.Emplace_GetRef();
				Attempt.BackendId = Candidate.ActualBackendId;
				Attempt.Disposition = Candidate.Disposition;
				Attempt.Detail = Candidate.Detail;
				Attempt.TypedDiagnostic = Candidate.TypedDiagnostic;
				Candidate.BackendAttempts.Reset();
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
					Result->BackendAttempts = MoveTemp(BackendAttempts);
				}
				else
				{
					*Result = MoveTemp(Candidate);
					Result->BackendAttempts = MoveTemp(BackendAttempts);
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
		auto Fail = [&OutOutput, OutError](const FString& Detail)
		{
			OutOutput.Diagnostics.TaskError = Detail;
			if (OutError != nullptr)
			{
				*OutError = Detail;
			}
			return false;
		};
		// Direct callers (GenerateStaticJITSourceTextForDiagnostics) skip
		// Generate() and therefore never copy RequestedBackendId onto the
		// output diagnostics. Provider validation treats an empty backend id
		// as InvalidDiagnosticString (code 65).
		if (!OutOutput.Diagnostics.RequestedBackendId.IsValid())
		{
			OutOutput.Diagnostics.RequestedBackendId = Request.RequestedBackendId;
		}
		if (OutOutput.Diagnostics.FallbackChain.IsEmpty()
			&& Request.RequestedBackendId.IsValid())
		{
			OutOutput.Diagnostics.FallbackChain.Add(Request.RequestedBackendId);
			if (Request.RequestedBackendId
				== FAngelscriptStaticJITBackendId::TypedAST())
			{
				OutOutput.Diagnostics.FallbackChain.Add(
					FAngelscriptStaticJITBackendId::Bytecode());
			}
		}

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
		if (PackageRequest.TargetProfile
			!= EAngelscriptJITTargetProfile::GameShipping)
		{
			TArray<FAngelscriptStableFunctionKey> EmittedFunctionKeys;
			EmittedFunctionKeys.Reserve(PackageRequest.Functions.Num());
			for (const FAngelscriptJITGenerationFunction& Function
				: PackageRequest.Functions)
			{
				EmittedFunctionKeys.Add(Function.FunctionKey);
			}
			PackageRequest.Diagnostics =
				BuildAngelscriptStaticJITGenerationDiagnostics(
					OutOutput, EmittedFunctionKeys);
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
		FString PackageError;
		if (!FAngelscriptJITGeneration::Generate(
			PackageRequest, OutOutput.ProviderOutput, &PackageError))
		{
			return Fail(PackageError.IsEmpty()
				? TEXT("StaticJIT Provider packaging failed.")
				: PackageError);
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

		// Every backend contributes to one module translation unit. Merge shared
		// include/guard lines individually so a TypedAST + Bytecode fallback does
		// not open the same AS_SKIP_JITTED_CODE guard twice but close it once.
		// Preserve duplicate lines inside one backend fragment; only lines already
		// contributed by an earlier backend are suppressed.
		TArray<FString> ExistingLines;
		Target.ParseIntoArrayLines(ExistingLines, false);
		TArray<FString> FragmentLines;
		Fragment.ParseIntoArrayLines(FragmentLines, false);
		for (const FString& Line : FragmentLines)
		{
			if (!Line.IsEmpty()
				&& !ExistingLines.ContainsByPredicate(
					[&Line](const FString& Existing)
					{
						return Existing.Equals(
							Line, ESearchCase::CaseSensitive);
					}))
			{
				Target += Line + TEXT("\n");
			}
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
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		TypedASTJITCapabilityProfile;
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot
		TypedASTJITExecutionRequirements;
	TConstArrayView<asIScriptModule*> CompiledSourceGraph;
	TConstArrayView<asIScriptModule*> EmitModuleSet;
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> GenerationSnapshot;
	EAngelscriptStaticJITCaptureProfile CaptureProfile =
		EAngelscriptStaticJITCaptureProfile::Bytecode;
};

/** Runtime-module-owned registry. Static backends are registered explicitly. */
ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITBackendRegistry&
GetAngelscriptStaticJITBackendRegistry();

/** Idempotently register the built-in bytecode backend. */
ANGELSCRIPTRUNTIME_API bool RegisterAngelscriptBytecodeJITBackend(
	FString* OutError = nullptr);

/** Idempotently register the task-local Canonical TypedAST backend. */
ANGELSCRIPTRUNTIME_API bool RegisterAngelscriptTypedASTJITBackend(
	FString* OutError = nullptr);

/** Explicit BackendId/capture generation entry point for programmatic callers. */
ANGELSCRIPTRUNTIME_API bool GenerateStaticJITProviderArtifacts(
	const FAngelscriptStaticJITProviderGenerationRequest& Request,
	FAngelscriptStaticJITGeneratorOutput& OutOutput,
	FString* OutError = nullptr);

#endif
