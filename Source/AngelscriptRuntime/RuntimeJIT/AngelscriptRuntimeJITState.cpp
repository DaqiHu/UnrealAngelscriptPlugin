#include "RuntimeJIT/AngelscriptRuntimeJITState.h"

#include "Async/Async.h"
#include "Containers/Queue.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"

namespace AngelscriptRuntimeJITState_Private
{
	using FSessionPtr = TSharedPtr<
		FAngelscriptRuntimeJITBackendSessionOwner,
		ESPMode::ThreadSafe>;

	static bool IsValidPlatform(
		const EAngelscriptRuntimeJITPlatform Platform)
	{
		return Platform == EAngelscriptRuntimeJITPlatform::Win64
			|| Platform == EAngelscriptRuntimeJITPlatform::Linux
			|| Platform == EAngelscriptRuntimeJITPlatform::Mac;
	}

	static bool IsValidConfiguration(
		const EAngelscriptRuntimeJITBuildConfiguration Configuration)
	{
		return Configuration
				== EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment
			|| Configuration
				== EAngelscriptRuntimeJITBuildConfiguration::GameDevelopment
			|| Configuration
				== EAngelscriptRuntimeJITBuildConfiguration::Shipping;
	}

	static bool IsValidPolicy(
		const EAngelscriptRuntimeJITCompilePolicy Policy)
	{
		return Policy == EAngelscriptRuntimeJITCompilePolicy::EagerSync
			|| Policy
				== EAngelscriptRuntimeJITCompilePolicy::EagerBackground
			|| Policy
				== EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall;
	}

	static bool IsValidSelection(
		const FAngelscriptRuntimeJITSelectionConfig& Selection)
	{
		return Selection.BackendId.IsValid()
			&& IsValidPlatform(Selection.Platform)
			&& IsValidConfiguration(Selection.Configuration)
			&& Selection.EngineNamespace != 0
			&& IsValidPolicy(Selection.CompilePolicy)
			&& Selection.BackendConfigurationGeneration != 0
			&& Selection.CancellationGeneration != 0;
	}

	static EAngelscriptRuntimeJITDiagnosticReason SelectionReason(
		const FAngelscriptRuntimeJITSessionCreateResult& CreateResult)
	{
		switch (CreateResult.Code)
		{
		case EAngelscriptRuntimeJITFactorySelectionCode::Selected:
			return EAngelscriptRuntimeJITDiagnosticReason::None;
		case EAngelscriptRuntimeJITFactorySelectionCode::DuplicateBackendId:
			return EAngelscriptRuntimeJITDiagnosticReason::DuplicateBackendId;
		case EAngelscriptRuntimeJITFactorySelectionCode::UnsupportedPlatform:
			return EAngelscriptRuntimeJITDiagnosticReason::UnsupportedPlatform;
		case EAngelscriptRuntimeJITFactorySelectionCode::
			UnsupportedConfiguration:
			return EAngelscriptRuntimeJITDiagnosticReason::
				UnsupportedConfiguration;
		case EAngelscriptRuntimeJITFactorySelectionCode::InvalidFactoryMetadata:
			return CreateResult.ValidationCode
					== EAngelscriptRuntimeJITContractValidationCode::
						UnsupportedRevision
				? EAngelscriptRuntimeJITDiagnosticReason::
					IncompatibleFactoryAbi
				: EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
		case EAngelscriptRuntimeJITFactorySelectionCode::MissingBackend:
		default:
			return EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
		}
	}

	struct FSelectedSession
	{
		FSessionPtr Session;
		EAngelscriptRuntimeJITDiagnosticReason Reason =
			EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
	};

	static FSelectedSession CreateSession(
		const FAngelscriptRuntimeJITSelectionConfig& Selection)
	{
		FSelectedSession Result;
		if (!IsValidSelection(Selection))
		{
			Result.Reason =
				EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
			return Result;
		}

		FAngelscriptRuntimeJITBackendSessionConfig SessionConfig;
		SessionConfig.BackendId = Selection.BackendId;
		SessionConfig.Platform = Selection.Platform;
		SessionConfig.Configuration = Selection.Configuration;
		SessionConfig.EngineNamespace = Selection.EngineNamespace;
		FAngelscriptRuntimeJITSessionCreateResult CreateResult =
			FAngelscriptRuntimeJITBackendSessionOwner::Create(SessionConfig);
		Result.Reason = SelectionReason(CreateResult);
		if (CreateResult.IsCreated())
		{
			Result.Session = FSessionPtr(CreateResult.Owner.Release());
			Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::None;
		}
		return Result;
	}

	struct FRequestIdentity
	{
		FAngelscriptRuntimeJITBackendId BackendId;
		uint64 EngineNamespace = 0;
		uint64 PublicationOrdinal = 0;
		uint64 BackendConfigurationGeneration = 0;
		uint64 CancellationGeneration = 0;
		uint32 EntryAbiRevision = 0;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableFunctionKey FunctionKey;
		FAngelscriptHash256 FunctionRevision;
		FAngelscriptHash256 EntryAbiHash;

		static FRequestIdentity From(
			const FAngelscriptRuntimeJITCompileSnapshotHeader& Header,
			const FAngelscriptRuntimeJITSelectionConfig& Selection)
		{
			FRequestIdentity Result;
			Result.BackendId = Header.BackendId;
			Result.EngineNamespace = Header.EngineNamespace;
			Result.PublicationOrdinal = Header.PublicationOrdinal;
			Result.BackendConfigurationGeneration =
				Selection.BackendConfigurationGeneration;
			Result.CancellationGeneration = Header.CancellationGeneration;
			Result.EntryAbiRevision = Header.EntryAbiRevision;
			Result.ModuleKey = Header.ModuleKey;
			Result.FunctionKey = Header.FunctionKey;
			Result.FunctionRevision = Header.FunctionRevision;
			Result.EntryAbiHash = Header.EntryAbiHash;
			return Result;
		}

		bool Matches(const FRequestIdentity& Other) const
		{
			return BackendId == Other.BackendId
				&& EngineNamespace == Other.EngineNamespace
				&& PublicationOrdinal == Other.PublicationOrdinal
				&& BackendConfigurationGeneration
					== Other.BackendConfigurationGeneration
				&& CancellationGeneration == Other.CancellationGeneration
				&& EntryAbiRevision == Other.EntryAbiRevision
				&& ModuleKey == Other.ModuleKey
				&& FunctionKey == Other.FunctionKey
				&& FunctionRevision == Other.FunctionRevision
				&& EntryAbiHash == Other.EntryAbiHash;
		}

		bool MatchesQuery(
			const FAngelscriptStableFunctionKey& InFunctionKey,
			const FAngelscriptHash256& InFunctionRevision,
			const uint64 InPublicationOrdinal) const
		{
			return FunctionKey == InFunctionKey
				&& FunctionRevision == InFunctionRevision
				&& PublicationOrdinal == InPublicationOrdinal;
		}
	};

	struct FCompileRequest
	{
		FRequestIdentity Identity;
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		FSessionPtr Session;
	};

	struct FCompileCompletion
	{
		FRequestIdentity Identity;
		FAngelscriptRuntimeJITCompileResult Result;
		FSessionPtr SessionKeepAlive;
	};

	struct FCompletionChannel
	{
		TQueue<FCompileCompletion, EQueueMode::Mpsc> Queue;
	};

	struct FFunctionRecord
	{
		FRequestIdentity Identity;
		EAngelscriptRuntimeJITFunctionState State =
			EAngelscriptRuntimeJITFunctionState::InvalidInput;
		EAngelscriptRuntimeJITDiagnosticReason Reason =
			EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
		TOptional<FAngelscriptRuntimeJITCompileSnapshot> LazySnapshot;
		FSessionPtr ActiveSession;
		FAngelscriptRuntimeJITPublishedCode Published;
	};

	static EAngelscriptRuntimeJITFunctionState StateForOutcome(
		const EAngelscriptRuntimeJITCompileOutcome Outcome)
	{
		switch (Outcome)
		{
		case EAngelscriptRuntimeJITCompileOutcome::Compiled:
			return EAngelscriptRuntimeJITFunctionState::Published;
		case EAngelscriptRuntimeJITCompileOutcome::Unsupported:
			return EAngelscriptRuntimeJITFunctionState::Unsupported;
		case EAngelscriptRuntimeJITCompileOutcome::Cancelled:
			return EAngelscriptRuntimeJITFunctionState::Cancelled;
		case EAngelscriptRuntimeJITCompileOutcome::Stale:
			return EAngelscriptRuntimeJITFunctionState::Stale;
		case EAngelscriptRuntimeJITCompileOutcome::BackendFailure:
			return EAngelscriptRuntimeJITFunctionState::BackendFailure;
		case EAngelscriptRuntimeJITCompileOutcome::InvalidInput:
		default:
			return EAngelscriptRuntimeJITFunctionState::InvalidInput;
		}
	}

	static FCompileCompletion Compile(FCompileRequest&& Request)
	{
		FCompileCompletion Completion;
		Completion.Identity = Request.Identity;
		Completion.SessionKeepAlive = Request.Session;
		Completion.Result = Request.Session->Compile(Request.Snapshot.GetView());
		return Completion;
	}
}

using namespace AngelscriptRuntimeJITState_Private;

struct FAngelscriptRuntimeJITRequestStateMachine::FImpl
{
	explicit FImpl(
		const FAngelscriptRuntimeJITSelectionConfig& InSelection)
		: Selection(InSelection)
		, CompletionChannel(
			MakeShared<FCompletionChannel, ESPMode::ThreadSafe>())
		, OperationsDrained(FPlatformProcess::GetSynchEventFromPool(true))
	{
		check(OperationsDrained != nullptr);
		OperationsDrained->Trigger();
		const FSelectedSession Selected = CreateSession(Selection);
		Session = Selected.Session;
		SessionReason = Selected.Reason;
	}

	~FImpl()
	{
		if (OperationsDrained != nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(OperationsDrained);
			OperationsDrained = nullptr;
		}
	}

	bool SnapshotMatchesSelection(
		const FAngelscriptRuntimeJITCompileSnapshotHeader& Header) const
	{
		return Header.BackendId == Selection.BackendId
			&& Header.Platform == Selection.Platform
			&& Header.Configuration == Selection.Configuration
			&& Header.EngineNamespace == Selection.EngineNamespace
			&& Header.CancellationGeneration
				== Selection.CancellationGeneration;
	}

	static FAngelscriptRuntimeJITObserveResult ExistingResult(
		const FFunctionRecord& Record)
	{
		FAngelscriptRuntimeJITObserveResult Result;
		Result.State = Record.State;
		Result.Reason = Record.Reason;
		switch (Record.State)
		{
		case EAngelscriptRuntimeJITFunctionState::Published:
			Result.Action =
				EAngelscriptRuntimeJITRequestAction::AlreadyPublished;
			break;
		case EAngelscriptRuntimeJITFunctionState::WaitingForFirstCall:
		case EAngelscriptRuntimeJITFunctionState::Compiling:
			Result.Action = EAngelscriptRuntimeJITRequestAction::Coalesced;
			break;
		default:
			Result.Action = EAngelscriptRuntimeJITRequestAction::Memoized;
			break;
		}
		return Result;
	}

	static FCompileRequest MakeCompileRequest(
		const FRequestIdentity& Identity,
		FAngelscriptRuntimeJITCompileSnapshot&& Snapshot,
		const FSessionPtr& InSession)
	{
		FCompileRequest Request;
		Request.Identity = Identity;
		Request.Snapshot = MoveTemp(Snapshot);
		Request.Session = InSession;
		return Request;
	}

	void BeginOperationLocked()
	{
		check(!bAdmissionClosed);
		if (ActiveOperationCount == 0)
		{
			OperationsDrained->Reset();
		}
		++ActiveOperationCount;
	}

	void EndOperation()
	{
		FScopeLock Lock(&StateMutex);
		check(ActiveOperationCount > 0);
		--ActiveOperationCount;
		if (ActiveOperationCount == 0)
		{
			OperationsDrained->Trigger();
		}
	}

	void StartBackgroundLocked(FCompileRequest&& Request)
	{
		const TSharedRef<FCompletionChannel, ESPMode::ThreadSafe> Channel =
			CompletionChannel;
		TFuture<void> Task = Async(
			EAsyncExecution::ThreadPool,
			[this, Channel, Request = MoveTemp(Request)]() mutable
			{
				ON_SCOPE_EXIT
				{
					EndOperation();
				};
				Channel->Queue.Enqueue(Compile(MoveTemp(Request)));
			});
		FScopeLock Lock(&TaskMutex);
		Tasks.Add(MoveTemp(Task));
	}

	void PruneCompletedTasks()
	{
		TArray<TFuture<void>> Completed;
		{
			FScopeLock Lock(&TaskMutex);
			for (int32 Index = Tasks.Num() - 1; Index >= 0; --Index)
			{
				if (!Tasks[Index].IsReady())
				{
					continue;
				}
				Completed.Add(MoveTemp(Tasks[Index]));
				Tasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
		for (TFuture<void>& Task : Completed)
		{
			Task.Get();
		}
	}

	void FinalizeCompletion(
		FCompileCompletion&& Completion,
		FAngelscriptRuntimeJITProcessResult& OutResult)
	{
		++OutResult.ProcessedCount;
		FScopeLock Lock(&StateMutex);
		FFunctionRecord* const Record = Functions.Find(
			Completion.Identity.FunctionKey.Hash.Value);
		if (bAdmissionClosed
			|| Record == nullptr
			|| !Record->Identity.Matches(Completion.Identity)
			|| Completion.Identity.BackendConfigurationGeneration
				!= Selection.BackendConfigurationGeneration
			|| Completion.Identity.CancellationGeneration
				!= Selection.CancellationGeneration)
		{
			++OutResult.StaleCount;
			return;
		}

		Record->ActiveSession.Reset();
		Record->State = StateForOutcome(Completion.Result.Outcome);
		Record->Reason = Completion.Result.Reason;
		if (Completion.Result.Outcome
			== EAngelscriptRuntimeJITCompileOutcome::Compiled)
		{
			FAngelscriptRuntimeJITPublishedCode Published;
			Published.BackendId = Completion.Identity.BackendId;
			Published.EngineNamespace = Completion.Identity.EngineNamespace;
			Published.PublicationOrdinal =
				Completion.Identity.PublicationOrdinal;
			Published.BackendConfigurationGeneration =
				Completion.Identity.BackendConfigurationGeneration;
			Published.CancellationGeneration =
				Completion.Identity.CancellationGeneration;
			Published.ModuleKey = Completion.Identity.ModuleKey;
			Published.FunctionKey = Completion.Identity.FunctionKey;
			Published.FunctionRevision =
				Completion.Identity.FunctionRevision;
			Published.EntryAbiHash = Completion.Identity.EntryAbiHash;
			Published.VMEntry = Completion.Result.VMEntry;
			Published.SessionKeepAlive = Completion.SessionKeepAlive;
			Published.CodeLease = MoveTemp(Completion.Result.CodeLease);
			Published.CompileLatencyMicroseconds =
				Completion.Result.CompileLatencyMicroseconds;
			Published.CodeSize = Completion.Result.CodeSize;
			Record->Published = MoveTemp(Published);
			++OutResult.PublishedCount;
		}
		else
		{
			++OutResult.TerminalCount;
		}
	}

	FAngelscriptRuntimeJITSelectionConfig Selection;
	FSessionPtr Session;
	EAngelscriptRuntimeJITDiagnosticReason SessionReason =
		EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
	TSharedRef<FCompletionChannel, ESPMode::ThreadSafe> CompletionChannel;
	mutable FCriticalSection StateMutex;
	TMap<FBlake3Hash, FFunctionRecord> Functions;
	FCriticalSection TaskMutex;
	TArray<TFuture<void>> Tasks;
	FEvent* OperationsDrained = nullptr;
	int32 ActiveOperationCount = 0;
	FCriticalSection ShutdownMutex;
	bool bAdmissionClosed = false;
	bool bShutdownComplete = false;
};

FAngelscriptRuntimeJITRequestStateMachine::
	FAngelscriptRuntimeJITRequestStateMachine(
		const FAngelscriptRuntimeJITSelectionConfig& InSelection)
	: Impl(MakeUnique<FImpl>(InSelection))
{
}

FAngelscriptRuntimeJITRequestStateMachine::
	~FAngelscriptRuntimeJITRequestStateMachine()
{
	Shutdown();
}

bool FAngelscriptRuntimeJITRequestStateMachine::Reconfigure(
	const FAngelscriptRuntimeJITSelectionConfig& InSelection)
{
	if (!IsValidSelection(InSelection))
	{
		return false;
	}

	{
		FScopeLock Lock(&Impl->StateMutex);
		if (Impl->bAdmissionClosed
			|| InSelection.EngineNamespace
				!= Impl->Selection.EngineNamespace
			|| InSelection.BackendConfigurationGeneration
				<= Impl->Selection.BackendConfigurationGeneration
			|| InSelection.CancellationGeneration
				<= Impl->Selection.CancellationGeneration)
		{
			return false;
		}
	}

	const FSelectedSession Selected = CreateSession(InSelection);
	TArray<FFunctionRecord> RetiredRecords;
	FSessionPtr RetiredSession;
	uint64 RetiredCancellationGeneration = 0;
	{
		FScopeLock Lock(&Impl->StateMutex);
		if (Impl->bAdmissionClosed
			|| InSelection.EngineNamespace
				!= Impl->Selection.EngineNamespace
			|| InSelection.BackendConfigurationGeneration
				<= Impl->Selection.BackendConfigurationGeneration
			|| InSelection.CancellationGeneration
				<= Impl->Selection.CancellationGeneration)
		{
			return false;
		}
		RetiredRecords.Reserve(Impl->Functions.Num());
		for (TPair<FBlake3Hash, FFunctionRecord>& Pair : Impl->Functions)
		{
			RetiredRecords.Add(MoveTemp(Pair.Value));
		}
		Impl->Functions.Empty();
		RetiredCancellationGeneration =
			Impl->Selection.CancellationGeneration;
		RetiredSession = MoveTemp(Impl->Session);
		Impl->Selection = InSelection;
		Impl->Session = Selected.Session;
		Impl->SessionReason = Selected.Reason;
	}

	if (RetiredSession.IsValid())
	{
		RetiredSession->Cancel(RetiredCancellationGeneration);
	}
	return true;
}

FAngelscriptRuntimeJITObserveResult
	FAngelscriptRuntimeJITRequestStateMachine::ObserveFunction(
		FAngelscriptRuntimeJITCompileSnapshot Snapshot)
{
	const FAngelscriptRuntimeJITContractValidationResult Validation =
		Snapshot.Validate();
	if (!Validation.IsValid())
	{
		FAngelscriptRuntimeJITObserveResult Result;
		Result.Action = EAngelscriptRuntimeJITRequestAction::Rejected;
		Result.State = EAngelscriptRuntimeJITFunctionState::InvalidInput;
		Result.Reason = Validation.Code
				== EAngelscriptRuntimeJITContractValidationCode::InvalidEntryAbi
			? EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch
			: EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
		return Result;
	}

	const FAngelscriptRuntimeJITCompileSnapshotHeader Header =
		Snapshot.GetHeader();
	FCompileRequest CompileRequest;
	FRequestIdentity RequestIdentity;
	TOptional<FFunctionRecord> RetiredRecord;
	FAngelscriptRuntimeJITObserveResult Result;
	EAngelscriptRuntimeJITCompilePolicy Policy =
		EAngelscriptRuntimeJITCompilePolicy::EagerSync;
	{
		FScopeLock Lock(&Impl->StateMutex);
		if (Impl->bAdmissionClosed)
		{
			Result.Action = EAngelscriptRuntimeJITRequestAction::Rejected;
			Result.State = EAngelscriptRuntimeJITFunctionState::Cancelled;
			Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::Cancelled;
			return Result;
		}
		if (!Impl->SnapshotMatchesSelection(Header))
		{
			Result.Action = EAngelscriptRuntimeJITRequestAction::Rejected;
			Result.State = Header.CancellationGeneration
					!= Impl->Selection.CancellationGeneration
				? EAngelscriptRuntimeJITFunctionState::Stale
				: EAngelscriptRuntimeJITFunctionState::InvalidInput;
			Result.Reason = Header.CancellationGeneration
					!= Impl->Selection.CancellationGeneration
				? EAngelscriptRuntimeJITDiagnosticReason::StaleRevision
				: EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
			return Result;
		}

		RequestIdentity =
			FRequestIdentity::From(Header, Impl->Selection);
		const FBlake3Hash MapKey = Header.FunctionKey.Hash.Value;
		if (FFunctionRecord* const Existing = Impl->Functions.Find(MapKey))
		{
			if (Existing->Identity.Matches(RequestIdentity))
			{
				return FImpl::ExistingResult(*Existing);
			}
			RetiredRecord.Emplace(MoveTemp(*Existing));
			Impl->Functions.Remove(MapKey);
		}

		FFunctionRecord Record;
		Record.Identity = RequestIdentity;
		Policy = Impl->Selection.CompilePolicy;
		if (!Impl->Session.IsValid())
		{
			Record.State =
				EAngelscriptRuntimeJITFunctionState::BackendUnavailable;
			Record.Reason = Impl->SessionReason;
			Result.Action = EAngelscriptRuntimeJITRequestAction::Started;
			Result.State = Record.State;
			Result.Reason = Record.Reason;
			Impl->Functions.Add(MapKey, MoveTemp(Record));
		}
		else if (Policy
			== EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall)
		{
			Record.State =
				EAngelscriptRuntimeJITFunctionState::WaitingForFirstCall;
			Record.Reason = EAngelscriptRuntimeJITDiagnosticReason::None;
			Record.LazySnapshot.Emplace(MoveTemp(Snapshot));
			Result.Action = EAngelscriptRuntimeJITRequestAction::Registered;
			Result.State = Record.State;
			Result.Reason = Record.Reason;
			Impl->Functions.Add(MapKey, MoveTemp(Record));
		}
		else
		{
			Record.State = EAngelscriptRuntimeJITFunctionState::Compiling;
			Record.Reason = EAngelscriptRuntimeJITDiagnosticReason::None;
			Record.ActiveSession = Impl->Session;
			CompileRequest = FImpl::MakeCompileRequest(
				RequestIdentity, MoveTemp(Snapshot), Impl->Session);
			Result.Action = EAngelscriptRuntimeJITRequestAction::Started;
			Result.State = Record.State;
			Result.Reason = Record.Reason;
			Impl->Functions.Add(MapKey, MoveTemp(Record));
			Impl->BeginOperationLocked();
			if (Policy
				== EAngelscriptRuntimeJITCompilePolicy::EagerBackground)
			{
				Impl->StartBackgroundLocked(MoveTemp(CompileRequest));
			}
		}
	}

	RetiredRecord.Reset();

	if (Policy == EAngelscriptRuntimeJITCompilePolicy::EagerBackground)
	{
		return Result;
	}
	if (!CompileRequest.Session.IsValid())
	{
		return Result;
	}
	ON_SCOPE_EXIT
	{
		Impl->EndOperation();
	};

	FAngelscriptRuntimeJITProcessResult ProcessResult;
	Impl->FinalizeCompletion(
		Compile(MoveTemp(CompileRequest)), ProcessResult);
	{
		FScopeLock Lock(&Impl->StateMutex);
		const FFunctionRecord* const Record = Impl->Functions.Find(
			Header.FunctionKey.Hash.Value);
		if (Record != nullptr
			&& Record->Identity.Matches(RequestIdentity))
		{
			Result.State = Record->State;
			Result.Reason = Record->Reason;
		}
		else
		{
			Result.State = EAngelscriptRuntimeJITFunctionState::Stale;
			Result.Reason =
				EAngelscriptRuntimeJITDiagnosticReason::StaleRevision;
		}
	}
	return Result;
}

FAngelscriptRuntimeJITLazyCallResult
	FAngelscriptRuntimeJITRequestStateMachine::OnLazyFirstCall(
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptHash256& FunctionRevision,
		const uint64 PublicationOrdinal)
{
	FAngelscriptRuntimeJITLazyCallResult Result;
	FCompileRequest Request;
	{
		FScopeLock Lock(&Impl->StateMutex);
		if (Impl->bAdmissionClosed)
		{
			Result.State = EAngelscriptRuntimeJITFunctionState::Cancelled;
			return Result;
		}
		FFunctionRecord* const Record = Impl->Functions.Find(
			FunctionKey.Hash.Value);
		if (Record == nullptr
			|| !Record->Identity.MatchesQuery(
				FunctionKey, FunctionRevision, PublicationOrdinal))
		{
			return Result;
		}
		Result.State = Record->State;
		if (Record->State
				!= EAngelscriptRuntimeJITFunctionState::WaitingForFirstCall
			|| !Record->LazySnapshot.IsSet()
			|| !Impl->Session.IsValid())
		{
			return Result;
		}

		FAngelscriptRuntimeJITCompileSnapshot Snapshot =
			MoveTemp(Record->LazySnapshot.GetValue());
		Record->LazySnapshot.Reset();
		Record->State = EAngelscriptRuntimeJITFunctionState::Compiling;
		Record->ActiveSession = Impl->Session;
		Request = FImpl::MakeCompileRequest(
			Record->Identity, MoveTemp(Snapshot), Impl->Session);
		Impl->BeginOperationLocked();
		Result.bCompileClaimed = true;
		Result.State = Record->State;
	}

	ON_SCOPE_EXIT
	{
		Impl->EndOperation();
	};
	Impl->CompletionChannel->Queue.Enqueue(Compile(MoveTemp(Request)));
	return Result;
}

FAngelscriptRuntimeJITProcessResult
	FAngelscriptRuntimeJITRequestStateMachine::ProcessPendingResults()
{
	FAngelscriptRuntimeJITProcessResult Result;
	FCompileCompletion Completion;
	while (Impl->CompletionChannel->Queue.Dequeue(Completion))
	{
		Impl->FinalizeCompletion(MoveTemp(Completion), Result);
		Completion = {};
	}
	Impl->PruneCompletedTasks();
	return Result;
}

bool FAngelscriptRuntimeJITRequestStateMachine::TryAcquirePublished(
	const FAngelscriptStableFunctionKey& FunctionKey,
	const FAngelscriptHash256& FunctionRevision,
	const uint64 PublicationOrdinal,
	FAngelscriptRuntimeJITPublishedCode& OutPublished) const
{
	OutPublished = {};
	FScopeLock Lock(&Impl->StateMutex);
	const FFunctionRecord* const Record = Impl->Functions.Find(
		FunctionKey.Hash.Value);
	if (Impl->bAdmissionClosed
		|| Record == nullptr
		|| Record->State != EAngelscriptRuntimeJITFunctionState::Published
		|| !Record->Identity.MatchesQuery(
			FunctionKey, FunctionRevision, PublicationOrdinal)
		|| !Record->Published.IsValid())
	{
		return false;
	}
	OutPublished = Record->Published;
	return true;
}

bool FAngelscriptRuntimeJITRequestStateMachine::RetireFunction(
	const FAngelscriptStableFunctionKey& FunctionKey,
	const uint64 PublicationOrdinal)
{
	TOptional<FFunctionRecord> RetiredRecord;
	{
		FScopeLock Lock(&Impl->StateMutex);
		FFunctionRecord* const Record = Impl->Functions.Find(
			FunctionKey.Hash.Value);
		if (Impl->bAdmissionClosed
			|| Record == nullptr
			|| Record->Identity.PublicationOrdinal != PublicationOrdinal)
		{
			return false;
		}
		RetiredRecord.Emplace(MoveTemp(*Record));
		Impl->Functions.Remove(FunctionKey.Hash.Value);
	}
	RetiredRecord.Reset();
	return true;
}

void FAngelscriptRuntimeJITRequestStateMachine::Shutdown()
{
	if (!Impl.IsValid())
	{
		return;
	}

	FScopeLock ShutdownLock(&Impl->ShutdownMutex);
	TArray<FFunctionRecord> RetiredRecords;
	FSessionPtr RetiredSession;
	uint64 RetiredCancellationGeneration = 0;
	{
		FScopeLock Lock(&Impl->StateMutex);
		if (Impl->bShutdownComplete)
		{
			return;
		}
		Impl->bAdmissionClosed = true;
		RetiredRecords.Reserve(Impl->Functions.Num());
		for (TPair<FBlake3Hash, FFunctionRecord>& Pair : Impl->Functions)
		{
			RetiredRecords.Add(MoveTemp(Pair.Value));
		}
		Impl->Functions.Empty();
		RetiredCancellationGeneration =
			Impl->Selection.CancellationGeneration;
		RetiredSession = MoveTemp(Impl->Session);
	}

	if (RetiredSession.IsValid())
	{
		RetiredSession->Cancel(RetiredCancellationGeneration);
	}
	Impl->OperationsDrained->Wait();

	TArray<TFuture<void>> PendingTasks;
	{
		FScopeLock Lock(&Impl->TaskMutex);
		PendingTasks = MoveTemp(Impl->Tasks);
	}
	for (TFuture<void>& Task : PendingTasks)
	{
		Task.Get();
	}

	FCompileCompletion Completion;
	while (Impl->CompletionChannel->Queue.Dequeue(Completion))
	{
		Completion = {};
	}

	RetiredRecords.Reset();
	RetiredSession.Reset();
	{
		FScopeLock Lock(&Impl->StateMutex);
		Impl->bShutdownComplete = true;
	}
}
