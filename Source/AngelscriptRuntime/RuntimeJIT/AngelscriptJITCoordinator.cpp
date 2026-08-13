#include "RuntimeJIT/AngelscriptJITCoordinator.h"

#include "AngelscriptEngine.h"
#if WITH_AS_DEBUGSERVER
#include "Debugging/AngelscriptDebugServer.h"
#endif
#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#endif
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Misc/ScopeLock.h"
#include "RuntimeJIT/AngelscriptRuntimeJITBindingContext.h"
#include "RuntimeJIT/AngelscriptRuntimeJITSnapshot.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"
#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"

namespace
{
	FThreadSafeCounter64 GNextJITCoordinatorEngineNamespace;

	bool RequiresRuntimeBytecodeVisibility(FAngelscriptEngine& Engine)
	{
#if WITH_AS_DEBUGSERVER
		if (Engine.DebugServer != nullptr
			&& (Engine.DebugServer->bIsDebugging
				|| Engine.DebugServer->DataBreakpoints.Num() != 0
				|| Engine.DebugServer->bBreakNextScriptLine))
		{
			return true;
		}
#endif
#if WITH_AS_COVERAGE
		if (FAngelscriptCodeCoverageExtension::GetForEngine(Engine) != nullptr)
		{
			return true;
		}
#endif
		return false;
	}
}

FAngelscriptJITCoordinator::FAngelscriptJITCoordinator(
	asIScriptEngine* InOwnerScriptEngine,
	const FAngelscriptJITCoordinatorConfig& InConfig)
	: OwnerScriptEngine(InOwnerScriptEngine)
	, Config(InConfig)
	, EngineNamespace(
		static_cast<uint64>(GNextJITCoordinatorEngineNamespace.Increment()))
{
	check(OwnerScriptEngine != nullptr);
	BindingsDrainedEvent = FPlatformProcess::GetSynchEventFromPool(true);
	check(BindingsDrainedEvent != nullptr);
	BindingsDrainedEvent->Trigger();
	auto AppendConfigurationError = [this](const TCHAR* Error)
	{
		if (!Config.ConfigurationError.IsEmpty())
		{
			Config.ConfigurationError += TEXT("; ");
		}
		Config.ConfigurationError += Error;
	};
	if (!FAngelscriptJITConfiguration::IsValidExecutionMode(
		Config.ExecutionMode))
	{
		Config.ExecutionMode = EAngelscriptJITExecutionMode::VMOnly;
		Config.bExecutionModeValid = false;
		Config.bRuntimeConfigurationValid = false;
		AppendConfigurationError(TEXT("Invalid JIT execution mode"));
	}
	if (!FAngelscriptJITConfiguration::IsValidCompilePolicy(
		Config.RuntimeCompilePolicy))
	{
		Config.bRuntimeConfigurationValid = false;
		AppendConfigurationError(TEXT("Invalid Runtime JIT compile policy"));
	}
	if (Config.RuntimeBackendId.IsValid()
		&& (!FAngelscriptJITConfiguration::IsValidRuntimePlatform(
				Config.RuntimePlatform)
			|| !FAngelscriptJITConfiguration::IsValidRuntimeConfiguration(
				Config.RuntimeConfiguration)))
	{
		Config.bRuntimeConfigurationValid = false;
		AppendConfigurationError(TEXT("Invalid Runtime JIT target"));
	}
	if (AllowsRuntime())
	{
		FAngelscriptRuntimeJITSelectionConfig Selection;
		Selection.BackendId = Config.RuntimeBackendId;
		Selection.Platform = Config.RuntimePlatform;
		Selection.Configuration = Config.RuntimeConfiguration;
		Selection.EngineNamespace = EngineNamespace;
		Selection.CompilePolicy = Config.RuntimeCompilePolicy;
		Selection.BackendConfigurationGeneration = 1;
		Selection.CancellationGeneration = 1;
		RuntimeState =
			MakeUnique<FAngelscriptRuntimeJITRequestStateMachine>(Selection);
	}
}

FAngelscriptJITCoordinator::~FAngelscriptJITCoordinator()
{
	Shutdown();
	FPlatformProcess::ReturnSynchEventToPool(BindingsDrainedEvent);
	BindingsDrainedEvent = nullptr;
}

void FAngelscriptJITCoordinator::Shutdown()
{
	FScopeLock ShutdownLock(&ShutdownMutex);
	if (bShutdownComplete)
	{
		return;
	}

	{
		FScopeLock Lock(&BindingMutex);
		bShuttingDown = true;
		UpdateBindingsDrainedEventLocked();
	}

	asIScriptEngine* const ScriptEngine = OwnerScriptEngine;
	if (ScriptEngine != nullptr
		&& ScriptEngine->GetJITCompiler() == this)
	{
		ScriptEngine->SetJITCompiler(nullptr);
	}

	if (RuntimeState.IsValid())
	{
		RuntimeState->Shutdown();
		RuntimeState.Reset();
	}
	{
		FScopeLock Lock(&RuntimeRecordMutex);
		RuntimeFunctionRecords.Empty();
	}

	for (;;)
	{
		{
			FScopeLock Lock(&BindingMutex);
			if (AreBindingContextsDrainedLocked())
			{
				break;
			}
		}
		BindingsDrainedEvent->Wait();
	}

	OwnerScriptEngine = nullptr;
	bShutdownComplete = true;
}

bool FAngelscriptJITCoordinator::IsInstalledOwner(
	const asIScriptFunction* ScriptFunction) const
{
	return ScriptFunction != nullptr
		&& ScriptFunction->GetEngine() == OwnerScriptEngine
		&& OwnerScriptEngine != nullptr
		&& OwnerScriptEngine->GetJITCompiler() == this;
}

void FAngelscriptJITCoordinator::OnFunctionReady(
	asIScriptFunction* ScriptFunction)
{
	if (!IsInstalledOwner(ScriptFunction))
	{
		return;
	}
	FRuntimeFunctionRecord Retired;
	bool bHadRetired = false;
	{
		FScopeLock Lock(&RuntimeRecordMutex);
		if (FRuntimeFunctionRecord* Existing =
			RuntimeFunctionRecords.Find(ScriptFunction))
		{
			Retired = *Existing;
			bHadRetired = true;
		}
		FRuntimeFunctionRecord& Record =
			RuntimeFunctionRecords.FindOrAdd(ScriptFunction);
		Record = {};
		Record.PublicationOrdinal = NextPublicationOrdinal++;
	}
	if (bHadRetired && RuntimeState.IsValid()
		&& !Retired.FunctionKey.Hash.IsZero())
	{
		RuntimeState->RetireFunction(
			Retired.FunctionKey, Retired.PublicationOrdinal);
	}

	// Native bindings are selected only through this Engine-local coordinator
	// after authoritative compilation. Function-ready observation itself never
	// reattaches a function by numeric FunctionId.
}

void FAngelscriptJITCoordinator::OnJITEntry(
	asIScriptFunction* ScriptFunction)
{
	if (!IsInstalledOwner(ScriptFunction) || !RuntimeState.IsValid()
		|| Config.RuntimeCompilePolicy
			!= EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall)
	{
		return;
	}
	FRuntimeFunctionRecord Record;
	{
		FScopeLock Lock(&RuntimeRecordMutex);
		const FRuntimeFunctionRecord* Existing =
			RuntimeFunctionRecords.Find(ScriptFunction);
		if (Existing == nullptr)
		{
			return;
		}
		Record = *Existing;
	}
	if (Record.FunctionKey.Hash.IsZero()
		|| Record.FunctionRevision.IsZero()
		|| Record.PublicationOrdinal == 0)
	{
		return;
	}
	RuntimeState->OnLazyFirstCall(
		Record.FunctionKey,
		Record.FunctionRevision,
		Record.PublicationOrdinal);
}

void FAngelscriptJITCoordinator::ReleaseFunctionBinding(
	asIScriptFunction*,
	const asSJITFunctionBinding& Binding)
{
	if (Binding.UserData != nullptr)
	{
		TSharedPtr<const FAngelscriptJITBindingContext,
			ESPMode::ThreadSafe> RetiredProvider;
		TSharedPtr<const FAngelscriptRuntimeJITBindingContext,
			ESPMode::ThreadSafe> RetiredRuntime;
		{
			FScopeLock Lock(&BindingMutex);
			ActiveProviderBindingContexts.RemoveAndCopyValue(
				Binding.UserData, RetiredProvider);
			ActiveRuntimeBindingContexts.RemoveAndCopyValue(
				Binding.UserData, RetiredRuntime);
			UpdateBindingsDrainedEventLocked();
		}
	}
}

bool FAngelscriptJITCoordinator::AreBindingContextsDrainedLocked() const
{
	return ActiveProviderBindingContexts.IsEmpty()
		&& ActiveRuntimeBindingContexts.IsEmpty();
}

void FAngelscriptJITCoordinator::UpdateBindingsDrainedEventLocked()
{
	if (AreBindingContextsDrainedLocked())
	{
		BindingsDrainedEvent->Trigger();
	}
	else
	{
		BindingsDrainedEvent->Reset();
	}
}

bool FAngelscriptJITCoordinator::PublishProviderSelection(
	asIScriptFunction* ScriptFunction,
	const FAngelscriptJITArtifactSelection& Selection,
	const FAngelscriptJITImmutableArtifactSetValidationResult*
		ImmutableValidation)
{
	if (!IsInstalledOwner(ScriptFunction))
	{
		return false;
	}
	if (!Selection.Provider.IsValid()
		|| !Selection.Provider->Entries.IsValidIndex(Selection.EntryIndex))
	{
		return false;
	}
	const FAngelscriptJITProviderCatalogEntry& Entry =
		Selection.Provider->Entries[Selection.EntryIndex];
	if (EnumHasAnyFlags(
			Entry.Flags,
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet)
		&& (ImmutableValidation == nullptr
			|| !ImmutableValidation->AllowsDirectSelection(Selection)))
	{
		return false;
	}
	TSharedRef<const FAngelscriptJITBindingContext, ESPMode::ThreadSafe>
		Context = MakeShared<FAngelscriptJITBindingContext,
			ESPMode::ThreadSafe>(Selection, ImmutableValidation);
	const asSJITFunctionBinding Binding = Context->BuildBinding();
	if (!Context->IsValid()
		|| (Binding.VMEntry == nullptr
			&& Binding.RawEntry == nullptr
			&& Binding.ParmsEntry == nullptr))
	{
		return false;
	}
	{
		FScopeLock Lock(&BindingMutex);
		if (bShuttingDown)
		{
			return false;
		}
		ActiveProviderBindingContexts.Add(Binding.UserData, Context);
		UpdateBindingsDrainedEventLocked();
	}
	if (ScriptFunction->SetJITBinding(Binding) != asSUCCESS)
	{
		TSharedPtr<const FAngelscriptJITBindingContext,
			ESPMode::ThreadSafe> Retired;
		{
			FScopeLock Lock(&BindingMutex);
			ActiveProviderBindingContexts.RemoveAndCopyValue(
				Binding.UserData, Retired);
			UpdateBindingsDrainedEventLocked();
		}
		return false;
	}
	return true;
}

bool FAngelscriptJITCoordinator::ClearProviderSelection(
	asIScriptFunction* ScriptFunction)
{
	if (!IsInstalledOwner(ScriptFunction))
	{
		return false;
	}
	const asSJITFunctionBinding Current = ScriptFunction->GetJITBinding();
	if (FAngelscriptJITBindingContext::FromBindingUserData(
		Current.UserData) == nullptr)
	{
		return false;
	}
	return ScriptFunction->SetJITBinding(asSJITFunctionBinding())
		== asSUCCESS;
}

uint64 FAngelscriptJITCoordinator::EnsurePublicationOrdinal(
	asIScriptFunction* ScriptFunction)
{
	FScopeLock Lock(&RuntimeRecordMutex);
	FRuntimeFunctionRecord& Record =
		RuntimeFunctionRecords.FindOrAdd(ScriptFunction);
	if (Record.PublicationOrdinal == 0)
	{
		Record.PublicationOrdinal = NextPublicationOrdinal++;
	}
	return Record.PublicationOrdinal;
}

bool FAngelscriptJITCoordinator::PublishRuntimeSelection(
	asIScriptFunction* ScriptFunction,
	const FAngelscriptRuntimeJITPublishedCode& Published)
{
	if (!IsInstalledOwner(ScriptFunction) || !Published.IsValid())
	{
		return false;
	}
	TSharedRef<const FAngelscriptRuntimeJITBindingContext,
		ESPMode::ThreadSafe> Context =
		MakeShared<FAngelscriptRuntimeJITBindingContext,
			ESPMode::ThreadSafe>(Published);
	const asSJITFunctionBinding Binding = Context->BuildBinding();
	if (!Context->IsValid() || Binding.VMEntry == nullptr
		|| Binding.RawEntry != nullptr || Binding.ParmsEntry != nullptr)
	{
		return false;
	}
	{
		FScopeLock Lock(&BindingMutex);
		if (bShuttingDown)
		{
			return false;
		}
		ActiveRuntimeBindingContexts.Add(Binding.UserData, Context);
		UpdateBindingsDrainedEventLocked();
	}
	if (ScriptFunction->SetJITBinding(Binding) != asSUCCESS)
	{
		TSharedPtr<const FAngelscriptRuntimeJITBindingContext,
			ESPMode::ThreadSafe> Retired;
		{
			FScopeLock Lock(&BindingMutex);
			ActiveRuntimeBindingContexts.RemoveAndCopyValue(
				Binding.UserData, Retired);
			UpdateBindingsDrainedEventLocked();
		}
		return false;
	}
	return true;
}

bool FAngelscriptJITCoordinator::ClearRuntimeSelection(
	asIScriptFunction* ScriptFunction)
{
	if (!IsInstalledOwner(ScriptFunction))
	{
		return false;
	}
	const asSJITFunctionBinding Current = ScriptFunction->GetJITBinding();
	if (FAngelscriptRuntimeJITBindingContext::FromBindingUserData(
		Current.UserData) == nullptr)
	{
		return false;
	}
	return ScriptFunction->SetJITBinding(asSJITFunctionBinding())
		== asSUCCESS;
}

bool FAngelscriptJITCoordinator::ProcessRuntimeSafePoint(
	FAngelscriptEngine& Engine)
{
	if (Engine.GetScriptEngine() != OwnerScriptEngine
		|| OwnerScriptEngine == nullptr
		|| OwnerScriptEngine->GetJITCompiler() != this)
	{
		return false;
	}

	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Routes = Engine.GetFunctionRouteSnapshot();
	if (!Routes.IsValid())
	{
		return true;
	}
	if (RuntimeState.IsValid())
	{
		RuntimeState->ProcessPendingResults();
	}

	TSet<asIScriptFunction*> LiveFunctions;
	for (const FAngelscriptFunctionRoute& Route : Routes->FunctionRoutes)
	{
		if (Route.Function != nullptr
			&& Route.Function->GetEngine() == OwnerScriptEngine)
		{
			LiveFunctions.Add(Route.Function);
		}
	}

	TArray<FRuntimeFunctionRecord> RetiredRecords;
	{
		FScopeLock Lock(&RuntimeRecordMutex);
		for (auto It = RuntimeFunctionRecords.CreateIterator(); It; ++It)
		{
			if (!LiveFunctions.Contains(It.Key()))
			{
				RetiredRecords.Add(It.Value());
				It.RemoveCurrent();
			}
		}
	}
	if (RuntimeState.IsValid())
	{
		for (const FRuntimeFunctionRecord& Retired : RetiredRecords)
		{
			if (!Retired.FunctionKey.Hash.IsZero())
			{
				RuntimeState->RetireFunction(
					Retired.FunctionKey, Retired.PublicationOrdinal);
			}
		}
	}

	bool bAnyBindingChanged = false;
	bool bPublicationSucceeded = true;
	const bool bRuntimeRequiresBytecodeVisibility =
		RequiresRuntimeBytecodeVisibility(Engine);
	TArray<asIScriptFunction*> PublishedFunctions;
	for (const FAngelscriptFunctionRoute& Route : Routes->FunctionRoutes)
	{
		asIScriptFunction* const Function = Route.Function;
		if (Function == nullptr
			|| Function->GetEngine() != OwnerScriptEngine
			|| !Route.bHasVerifiedArtifactIdentity
			|| Route.ModuleKey.Hash.IsZero()
			|| Route.Identity.FunctionKey.Hash.IsZero())
		{
			continue;
		}

		const asSJITFunctionBinding CurrentBinding =
			Function->GetJITBinding();
		const bool bHasStaticAOT =
			FAngelscriptJITBindingContext::FromBindingUserData(
				CurrentBinding.UserData) != nullptr;
		const bool bShouldUseRuntime = !bRuntimeRequiresBytecodeVisibility
			&& RuntimeState.IsValid()
			&& (Config.ExecutionMode
					== EAngelscriptJITExecutionMode::RuntimeOnly
				|| (Config.ExecutionMode
						== EAngelscriptJITExecutionMode::Auto
					&& !bHasStaticAOT));
		const uint64 PublicationOrdinal =
			EnsurePublicationOrdinal(Function);

		if (!bShouldUseRuntime)
		{
			bAnyBindingChanged |= ClearRuntimeSelection(Function);
			if (RuntimeState.IsValid())
			{
				RuntimeState->RetireFunction(
					Route.Identity.FunctionKey, PublicationOrdinal);
			}
			continue;
		}

		FAngelscriptRuntimeJITSnapshotCaptureInput CaptureInput;
		CaptureInput.Function = Function;
		CaptureInput.Route = &Route;
		CaptureInput.BackendId = Config.RuntimeBackendId;
		CaptureInput.Platform = Config.RuntimePlatform;
		CaptureInput.Configuration = Config.RuntimeConfiguration;
		CaptureInput.EngineNamespace = EngineNamespace;
		CaptureInput.PublicationOrdinal = PublicationOrdinal;
		CaptureInput.CancellationGeneration = 1;
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			FAngelscriptRuntimeJITCompileSnapshot::Capture(
				CaptureInput, Snapshot);
		if (!Capture.IsCaptured())
		{
			bAnyBindingChanged |= ClearRuntimeSelection(Function);
			RuntimeState->RetireFunction(
				Route.Identity.FunctionKey, PublicationOrdinal);
			continue;
		}

		const FAngelscriptHash256 FunctionRevision =
			Snapshot.GetHeader().FunctionRevision;
		{
			FScopeLock Lock(&RuntimeRecordMutex);
			if (FRuntimeFunctionRecord* Record =
				RuntimeFunctionRecords.Find(Function);
				Record != nullptr
				&& Record->PublicationOrdinal == PublicationOrdinal)
			{
				Record->FunctionKey = Route.Identity.FunctionKey;
				Record->FunctionRevision = FunctionRevision;
			}
		}

		FAngelscriptRuntimeJITPublishedCode Published;
		if (!RuntimeState->TryAcquirePublished(
			Route.Identity.FunctionKey,
			FunctionRevision,
			PublicationOrdinal,
			Published))
		{
			RuntimeState->ObserveFunction(MoveTemp(Snapshot));
			if (!RuntimeState->TryAcquirePublished(
				Route.Identity.FunctionKey,
				FunctionRevision,
				PublicationOrdinal,
				Published))
			{
				continue;
			}
		}

		const FAngelscriptRuntimeJITBindingContext* CurrentRuntime =
			FAngelscriptRuntimeJITBindingContext::FromBindingUserData(
				Function->GetJITBinding().UserData);
		if (CurrentRuntime != nullptr)
		{
			const FAngelscriptRuntimeJITPublishedCode& Current =
				CurrentRuntime->GetPublished();
			if (Current.FunctionKey == Published.FunctionKey
				&& Current.FunctionRevision == Published.FunctionRevision
				&& Current.PublicationOrdinal
					== Published.PublicationOrdinal)
			{
				continue;
			}
		}
		if (PublishRuntimeSelection(Function, Published))
		{
			bAnyBindingChanged = true;
			PublishedFunctions.Add(Function);
		}
		else
		{
			bPublicationSucceeded = false;
		}
	}

	if (bAnyBindingChanged
		&& !Engine.RefreshFunctionRouteSnapshotAfterStaticJITChange(true))
	{
		for (asIScriptFunction* Function : PublishedFunctions)
		{
			ClearRuntimeSelection(Function);
		}
		return false;
	}
	return bPublicationSucceeded;
}
