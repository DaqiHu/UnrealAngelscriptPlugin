#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Artifacts/AngelscriptArtifactReference.h"
#include "RuntimeJIT/AngelscriptJITConfiguration.h"
#include "RuntimeJIT/AngelscriptRuntimeJITState.h"

struct FAngelscriptEngine;
struct FAngelscriptJITDiagnosticSnapshot;

/**
 * The one JIT lifecycle owner installed on an AngelScript Engine.
 *
 * Static AOT publication and Runtime compilation both attach through this
 * coordinator. Backend factories and generators never install themselves as
 * an asIJITCompiler.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITCoordinator final
	: public asIJITCompiler
{
public:
	explicit FAngelscriptJITCoordinator(
		asIScriptEngine* InOwnerScriptEngine,
		const FAngelscriptJITCoordinatorConfig& InConfig = {});
	~FAngelscriptJITCoordinator() override;

	/**
	 * Stop new publication, detach from the owner Engine, drain Runtime work,
	 * and wait until every maintained-fork Binding execution reader releases
	 * its retired Binding. Safe to call more than once.
	 */
	void Shutdown();

	void OnFunctionReady(asIScriptFunction* ScriptFunction) override;
	void OnJITEntry(asIScriptFunction* ScriptFunction) override;
	void ReleaseFunctionBinding(
		asIScriptFunction* ScriptFunction,
		const asSJITFunctionBinding& Binding) override;

	bool PublishProviderSelection(
		asIScriptFunction* ScriptFunction,
		const struct FAngelscriptJITArtifactSelection& Selection,
		const class FAngelscriptJITImmutableArtifactSetValidationResult*
			ImmutableValidation = nullptr);

	bool ClearProviderSelection(asIScriptFunction* ScriptFunction);

	/** Capture/consume Runtime work and publish exact results at an Engine-safe
	 * route boundary. Returns true when no Runtime Binding publication failed. */
	bool ProcessRuntimeSafePoint(FAngelscriptEngine& Engine);

#if !UE_BUILD_SHIPPING
	/** Copy pointer-free coordinator/runtime observations for diagnostics. */
	FAngelscriptJITDiagnosticSnapshot CaptureDiagnostics(
		FAngelscriptEngine& Engine);
#endif

	asIScriptEngine* GetOwnerScriptEngine() const
	{
		return OwnerScriptEngine;
	}

	const FAngelscriptJITCoordinatorConfig& GetConfig() const
	{
		return Config;
	}

	uint64 GetEngineNamespace() const
	{
		return EngineNamespace;
	}

	bool AllowsStaticAOT() const
	{
		return Config.bExecutionModeValid
			&& (Config.ExecutionMode == EAngelscriptJITExecutionMode::Auto
				|| Config.ExecutionMode
					== EAngelscriptJITExecutionMode::StaticAOTOnly);
	}

	bool AllowsRuntime() const
	{
		return Config.bExecutionModeValid
			&& Config.bRuntimeConfigurationValid
			&& Config.RuntimeBackendId.IsValid()
			&& (Config.ExecutionMode == EAngelscriptJITExecutionMode::Auto
				|| Config.ExecutionMode
					== EAngelscriptJITExecutionMode::RuntimeOnly);
	}

	struct FSemanticDependentRecord
	{
		EAngelscriptArtifactSemanticDependencyKind Kind =
			EAngelscriptArtifactSemanticDependencyKind::Invalid;
		FAngelscriptHash256 TargetStableKey;
		TArray<FAngelscriptStableFunctionKey> Dependents;
	};

	void ReplaceSemanticDependents(
		TArray<FSemanticDependentRecord>&& InDependents);

	bool CollectSemanticDependents(
		EAngelscriptArtifactSemanticDependencyKind Kind,
		const FAngelscriptHash256& TargetStableKey,
		TArray<FAngelscriptStableFunctionKey>& OutDependents) const;

private:
	struct FRuntimeFunctionRecord
	{
		FAngelscriptStableFunctionKey FunctionKey;
		FAngelscriptHash256 FunctionRevision;
		uint64 PublicationOrdinal = 0;
	};

	bool IsInstalledOwner(const asIScriptFunction* ScriptFunction) const;
	uint64 EnsurePublicationOrdinal(asIScriptFunction* ScriptFunction);
	bool PublishRuntimeSelection(
		asIScriptFunction* ScriptFunction,
		const FAngelscriptRuntimeJITPublishedCode& Published);
	bool ClearRuntimeSelection(asIScriptFunction* ScriptFunction);
	bool AreBindingContextsDrainedLocked() const;
	void UpdateBindingsDrainedEventLocked();

	asIScriptEngine* OwnerScriptEngine = nullptr;
	FAngelscriptJITCoordinatorConfig Config;
	uint64 EngineNamespace = 0;
	FCriticalSection BindingMutex;
	FEvent* BindingsDrainedEvent = nullptr;
	FCriticalSection ShutdownMutex;
	bool bShuttingDown = false;
	bool bShutdownComplete = false;
	TMap<const void*, TSharedPtr<const class FAngelscriptJITBindingContext,
		ESPMode::ThreadSafe>> ActiveProviderBindingContexts;
	TMap<const void*, TSharedPtr<const class FAngelscriptRuntimeJITBindingContext,
		ESPMode::ThreadSafe>> ActiveRuntimeBindingContexts;
	FCriticalSection RuntimeRecordMutex;
	TMap<asIScriptFunction*, FRuntimeFunctionRecord> RuntimeFunctionRecords;
	uint64 NextPublicationOrdinal = 1;
	TUniquePtr<FAngelscriptRuntimeJITRequestStateMachine> RuntimeState;
	TArray<FSemanticDependentRecord> SemanticDependents;
};
