#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptFunctionRoute.h"
#include "RuntimeJIT/AngelscriptJITConfiguration.h"

struct FAngelscriptEngine;

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITFunctionDiagnosticRecord
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 FunctionRevision;
	FString CanonicalDeclaration;
	EAngelscriptJITExecutionTier RequestedTier =
		EAngelscriptJITExecutionTier::VM;
	EAngelscriptJITExecutionTier ActualTier =
		EAngelscriptJITExecutionTier::VM;
	EAngelscriptArtifactMatchResult StaticMatchResult =
		EAngelscriptArtifactMatchResult::MissingProviderEntry;
	EAngelscriptRuntimeJITFunctionProfile RuntimeProfile =
		EAngelscriptRuntimeJITFunctionProfile::Unknown;
	EAngelscriptRuntimeJITFunctionState RuntimeState =
		EAngelscriptRuntimeJITFunctionState::Missing;
	EAngelscriptRuntimeJITDiagnosticReason RuntimeReason =
		EAngelscriptRuntimeJITDiagnosticReason::None;
	uint64 RuntimeCompileAttemptCount = 0;
	uint64 RuntimeExecutionCount = 0;
	uint64 CompileLatencyMicroseconds = 0;
	uint64 CodeSize = 0;
	int32 CodeLeaseReferenceCount = 0;
};

/** Deterministic, pointer-free non-Shipping coordinator observation. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITDiagnosticSnapshot
{
	static constexpr uint32 CurrentSchemaVersion = 1;

	uint32 SchemaVersion = CurrentSchemaVersion;
	bool bHasEngine = false;
	bool bHasCoordinator = false;
	EAngelscriptJITExecutionMode ExecutionMode =
		EAngelscriptJITExecutionMode::VMOnly;
	bool bExecutionModeValid = true;
	bool bRuntimeConfigurationValid = true;
	FString ConfigurationError;
	FAngelscriptRuntimeJITBackendId RuntimeBackendId;
	EAngelscriptRuntimeJITCompilePolicy RuntimeCompilePolicy =
		EAngelscriptRuntimeJITCompilePolicy::EagerSync;
	bool bRuntimeFactoryAvailable = false;
	bool bRuntimeSessionAvailable = false;
	EAngelscriptRuntimeJITDiagnosticReason RuntimeSessionReason =
		EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
	EAngelscriptRuntimeJITDiagnosticReason RuntimeGateReason =
		EAngelscriptRuntimeJITDiagnosticReason::None;
	uint64 TotalRuntimeCompileAttempts = 0;
	uint64 TotalRuntimeResults = 0;
	uint64 StaleRuntimeResultCount = 0;
	uint64 CancelledRuntimeResultCount = 0;
	uint64 RuntimeCancellationRequestCount = 0;
	int32 ActiveRuntimeOperationCount = 0;
	int32 LiveRuntimeCodeLeaseCount = 0;
	int32 RetiredRuntimeCodeLeaseCount = 0;
	TArray<FAngelscriptJITFunctionDiagnosticRecord> Functions;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITDiagnostics
{
	struct FJsonResult
	{
		FString Json;
		FString Detail;

		bool IsSuccess() const
		{
			return !Json.IsEmpty() && Detail.IsEmpty();
		}
	};

	static FAngelscriptJITDiagnosticSnapshot CaptureSnapshot(
		FAngelscriptEngine* Engine = nullptr);
	static FJsonResult Serialize(
		const FAngelscriptJITDiagnosticSnapshot& Snapshot);
	static FJsonResult CaptureJson(FAngelscriptEngine* Engine = nullptr);
	static void DumpDiagnostics(const TArray<FString>& Args);
};
