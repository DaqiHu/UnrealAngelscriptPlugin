#include "RuntimeJIT/AngelscriptJITDiagnostics.h"

#include "AngelscriptEngine.h"
#include "RuntimeJIT/AngelscriptJITCoordinator.h"
#include "StaticJIT/StaticJITConfig.h"

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogJITDiagnostics, Log, All);

namespace
{
	using FDiagnosticJsonWriter =
		TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	const TCHAR* TierName(const EAngelscriptJITExecutionTier Tier)
	{
		switch (Tier)
		{
		case EAngelscriptJITExecutionTier::StaticAOT:
			return TEXT("StaticAOT");
		case EAngelscriptJITExecutionTier::Runtime:
			return TEXT("Runtime");
		case EAngelscriptJITExecutionTier::VM:
		default:
			return TEXT("VM");
		}
	}

	const TCHAR* ModeName(const EAngelscriptJITExecutionMode Mode)
	{
		switch (Mode)
		{
		case EAngelscriptJITExecutionMode::Auto:
			return TEXT("Auto");
		case EAngelscriptJITExecutionMode::StaticAOTOnly:
			return TEXT("StaticAOTOnly");
		case EAngelscriptJITExecutionMode::RuntimeOnly:
			return TEXT("RuntimeOnly");
		case EAngelscriptJITExecutionMode::VMOnly:
		default:
			return TEXT("VMOnly");
		}
	}

	const TCHAR* PolicyName(const EAngelscriptRuntimeJITCompilePolicy Policy)
	{
		switch (Policy)
		{
		case EAngelscriptRuntimeJITCompilePolicy::EagerBackground:
			return TEXT("EagerBackground");
		case EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall:
			return TEXT("LazyFirstCall");
		case EAngelscriptRuntimeJITCompilePolicy::EagerSync:
		default:
			return TEXT("EagerSync");
		}
	}

	const TCHAR* StateName(const EAngelscriptRuntimeJITFunctionState State)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Missing"), TEXT("WaitingForFirstCall"), TEXT("Compiling"),
			TEXT("Published"), TEXT("Unsupported"), TEXT("Cancelled"),
			TEXT("BackendFailure"), TEXT("InvalidInput"), TEXT("Stale"),
			TEXT("BackendUnavailable"),
		};
		const uint8 Index = static_cast<uint8>(State);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* ReasonName(const EAngelscriptRuntimeJITDiagnosticReason Reason)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("None"), TEXT("BackendUnavailable"),
			TEXT("DuplicateBackendId"), TEXT("IncompatibleFactoryAbi"),
			TEXT("UnsupportedPlatform"), TEXT("UnsupportedConfiguration"),
			TEXT("UnsupportedOpcode"), TEXT("UnsupportedManagedValue"),
			TEXT("UnsupportedReceiver"), TEXT("SuspendOrLatent"),
			TEXT("ExceptionCleanup"), TEXT("DebuggerActive"),
			TEXT("CoverageActive"), TEXT("EntryAbiMismatch"),
			TEXT("MalformedSnapshot"), TEXT("Cancelled"),
			TEXT("StaleRevision"), TEXT("BackendFailure"),
			TEXT("InvalidBackendResult"),
		};
		const uint16 Index = static_cast<uint16>(Reason);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* ProfileName(const EAngelscriptRuntimeJITFunctionProfile Profile)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Unknown"), TEXT("ScalarWholeFunction"),
			TEXT("ManagedValues"), TEXT("SuspendOrLatent"),
			TEXT("ExceptionCleanup"),
		};
		const uint8 Index = static_cast<uint8>(Profile);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	const TCHAR* MatchName(const EAngelscriptArtifactMatchResult Match)
	{
		static constexpr const TCHAR* Names[] = {
			TEXT("Exact"), TEXT("MissingArtifactIdentity"),
			TEXT("MissingProviderEntry"), TEXT("ContentMismatch"),
			TEXT("DebugMismatch"), TEXT("ProfileMismatch"),
			TEXT("EnvironmentMismatch"), TEXT("EntryAbiMismatch"),
			TEXT("MissingReference"), TEXT("AmbiguousReference"),
			TEXT("WrongReferenceKind"), TEXT("ReferenceAbiMismatch"),
			TEXT("StaleProviderGeneration"), TEXT("DuplicateProviderEntry"),
			TEXT("AmbiguousExactProvider"), TEXT("ArtifactSetMismatch"),
			TEXT("InvalidProvider"), TEXT("MissingEntryPoint"),
		};
		const uint8 Index = static_cast<uint8>(Match);
		return Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("Unknown");
	}

	FString SerializeSnapshot(const FAngelscriptJITDiagnosticSnapshot& Snapshot)
	{
		FString Json;
		TSharedRef<FDiagnosticJsonWriter> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(
				&Json);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("schemaVersion"), Snapshot.SchemaVersion);
		Writer->WriteValue(TEXT("hasEngine"), Snapshot.bHasEngine);
		Writer->WriteValue(TEXT("hasCoordinator"), Snapshot.bHasCoordinator);
		Writer->WriteValue(TEXT("executionMode"), ModeName(Snapshot.ExecutionMode));
		Writer->WriteValue(TEXT("executionModeValid"),
			Snapshot.bExecutionModeValid);
		Writer->WriteValue(TEXT("runtimeConfigurationValid"),
			Snapshot.bRuntimeConfigurationValid);
		Writer->WriteValue(TEXT("configurationError"),
			Snapshot.ConfigurationError);
		Writer->WriteValue(TEXT("runtimeBackendId"),
			Snapshot.RuntimeBackendId.ToString());
		Writer->WriteValue(TEXT("runtimeCompilePolicy"),
			PolicyName(Snapshot.RuntimeCompilePolicy));
		Writer->WriteValue(TEXT("runtimeFactoryAvailable"),
			Snapshot.bRuntimeFactoryAvailable);
		Writer->WriteValue(TEXT("runtimeSessionAvailable"),
			Snapshot.bRuntimeSessionAvailable);
		Writer->WriteValue(TEXT("runtimeSessionReason"),
			ReasonName(Snapshot.RuntimeSessionReason));
		Writer->WriteValue(TEXT("runtimeGateReason"),
			ReasonName(Snapshot.RuntimeGateReason));
		Writer->WriteValue(TEXT("totalRuntimeCompileAttempts"),
			static_cast<int64>(Snapshot.TotalRuntimeCompileAttempts));
		Writer->WriteValue(TEXT("totalRuntimeResults"),
			static_cast<int64>(Snapshot.TotalRuntimeResults));
		Writer->WriteValue(TEXT("staleRuntimeResultCount"),
			static_cast<int64>(Snapshot.StaleRuntimeResultCount));
		Writer->WriteValue(TEXT("cancelledRuntimeResultCount"),
			static_cast<int64>(Snapshot.CancelledRuntimeResultCount));
		Writer->WriteValue(TEXT("runtimeCancellationRequestCount"),
			static_cast<int64>(Snapshot.RuntimeCancellationRequestCount));
		Writer->WriteValue(TEXT("activeRuntimeOperationCount"),
			Snapshot.ActiveRuntimeOperationCount);
		Writer->WriteValue(TEXT("liveRuntimeCodeLeaseCount"),
			Snapshot.LiveRuntimeCodeLeaseCount);
		Writer->WriteValue(TEXT("retiredRuntimeCodeLeaseCount"),
			Snapshot.RetiredRuntimeCodeLeaseCount);
		Writer->WriteArrayStart(TEXT("functions"));
		for (const FAngelscriptJITFunctionDiagnosticRecord& Function :
			Snapshot.Functions)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("moduleKey"),
				Function.ModuleKey.Hash.ToHexString());
			Writer->WriteValue(TEXT("functionKey"),
				Function.FunctionKey.Hash.ToHexString());
			Writer->WriteValue(TEXT("functionRevision"),
				Function.FunctionRevision.ToHexString());
			Writer->WriteValue(TEXT("canonicalDeclaration"),
				Function.CanonicalDeclaration);
			Writer->WriteValue(TEXT("requestedTier"),
				TierName(Function.RequestedTier));
			Writer->WriteValue(TEXT("actualTier"),
				TierName(Function.ActualTier));
			Writer->WriteValue(TEXT("staticMatchResult"),
				MatchName(Function.StaticMatchResult));
			Writer->WriteValue(TEXT("runtimeProfile"),
				ProfileName(Function.RuntimeProfile));
			Writer->WriteValue(TEXT("runtimeState"),
				StateName(Function.RuntimeState));
			Writer->WriteValue(TEXT("runtimeReason"),
				ReasonName(Function.RuntimeReason));
			Writer->WriteValue(TEXT("runtimeCompileAttemptCount"),
				static_cast<int64>(Function.RuntimeCompileAttemptCount));
			Writer->WriteValue(TEXT("runtimeExecutionCount"),
				static_cast<int64>(Function.RuntimeExecutionCount));
			Writer->WriteValue(TEXT("compileLatencyMicroseconds"),
				static_cast<int64>(Function.CompileLatencyMicroseconds));
			Writer->WriteValue(TEXT("codeSize"),
				static_cast<int64>(Function.CodeSize));
			Writer->WriteValue(TEXT("codeLeaseReferenceCount"),
				Function.CodeLeaseReferenceCount);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		return Writer->Close() ? Json : FString();
	}

#if AS_WITH_STATIC_JIT_DIAGNOSTICS
	FAutoConsoleCommand GJITDumpDiagnosticsCommand(
		TEXT("as.JIT.DumpDiagnostics"),
		TEXT("Dump deterministic coordinator/Runtime JIT JSON. Usage: as.JIT.DumpDiagnostics [-Function=<declaration-or-stable-key>] [-Output=<filename>]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(
			&FAngelscriptJITDiagnostics::DumpDiagnostics));
#endif
}

FAngelscriptJITDiagnosticSnapshot FAngelscriptJITDiagnostics::CaptureSnapshot(
	FAngelscriptEngine* Engine)
{
#if UE_BUILD_SHIPPING
	return {};
#else
	FAngelscriptEngine* const CurrentEngine = Engine != nullptr
		? Engine : FAngelscriptEngine::TryGetCurrentEngine();
	if (CurrentEngine == nullptr)
	{
		return {};
	}
	if (FAngelscriptJITCoordinator* const Coordinator =
		CurrentEngine->GetJITCoordinator())
	{
		return Coordinator->CaptureDiagnostics(*CurrentEngine);
	}
	FAngelscriptJITDiagnosticSnapshot Snapshot;
	Snapshot.bHasEngine = true;
	return Snapshot;
#endif
}

FAngelscriptJITDiagnostics::FJsonResult FAngelscriptJITDiagnostics::Serialize(
	const FAngelscriptJITDiagnosticSnapshot& Snapshot)
{
	FJsonResult Result;
	Result.Json = SerializeSnapshot(Snapshot);
	if (Result.Json.IsEmpty())
	{
		Result.Detail = TEXT("Failed to serialize JIT diagnostics JSON.");
	}
	return Result;
}

FAngelscriptJITDiagnostics::FJsonResult FAngelscriptJITDiagnostics::CaptureJson(
	FAngelscriptEngine* Engine)
{
	return Serialize(CaptureSnapshot(Engine));
}

void FAngelscriptJITDiagnostics::DumpDiagnostics(const TArray<FString>& Args)
{
#if AS_WITH_STATIC_JIT_DIAGNOSTICS
	FString FunctionQuery;
	FString OutputFilename;
	for (const FString& Arg : Args)
	{
		if (Arg.StartsWith(TEXT("-Function=")))
		{
			FunctionQuery = Arg.Mid(10);
		}
		else if (Arg.StartsWith(TEXT("-Output=")))
		{
			OutputFilename = Arg.Mid(8);
		}
		else
		{
			UE_LOG(LogJITDiagnostics, Error,
				TEXT("Usage: as.JIT.DumpDiagnostics [-Function=<declaration-or-stable-key>] [-Output=<filename>]"));
			return;
		}
	}

	FAngelscriptJITDiagnosticSnapshot Snapshot = CaptureSnapshot();
	if (!FunctionQuery.IsEmpty())
	{
		Snapshot.Functions.RemoveAll(
			[&FunctionQuery](
				const FAngelscriptJITFunctionDiagnosticRecord& Function)
			{
				return !Function.CanonicalDeclaration.Contains(FunctionQuery)
					&& !Function.FunctionKey.Hash.ToHexString().Contains(
						FunctionQuery);
			});
	}
	const FJsonResult Result = Serialize(Snapshot);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogJITDiagnostics, Error, TEXT("%s"), *Result.Detail);
		return;
	}
	if (OutputFilename.IsEmpty())
	{
		UE_LOG(LogJITDiagnostics, Log, TEXT("%s"), *Result.Json);
		return;
	}
	if (!FFileHelper::SaveStringToFile(
		Result.Json,
		*OutputFilename,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogJITDiagnostics, Error,
			TEXT("JIT diagnostics failed to write JSON to '%s'."),
			*OutputFilename);
	}
#endif
}
