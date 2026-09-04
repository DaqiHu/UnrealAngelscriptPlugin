#include "Misc/CoreDelegates.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Compilation/AngelscriptCompilationEvents.h"

#include "CQTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerBuilderIntegrationTests,
	"Angelscript.TestModule.Compiler.BuilderIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static const FAngelscriptCompilationEvent* FindFirstEvent(
	const TArray<FAngelscriptCompilationEvent>& Events,
	EAngelscriptCompilationEventType EventType)
{
	return Events.FindByPredicate(
		[EventType](const FAngelscriptCompilationEvent& Event)
		{
			return Event.Type == EventType;
		});
}

static const FAngelscriptCompilationEvent* FindFirstEventForModule(
	const TArray<FAngelscriptCompilationEvent>& Events,
	EAngelscriptCompilationEventType EventType,
	const FString& ModuleName)
{
	return Events.FindByPredicate(
		[EventType, &ModuleName](const FAngelscriptCompilationEvent& Event)
		{
			return Event.Type == EventType
				&& Event.ModuleNames.Contains(ModuleName);
		});
}

static int32 FindFirstPhaseIndex(
	const TArray<FAngelscriptCompilationEvent>& Events,
	FName Phase)
{
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		if (Events[Index].Phase == Phase)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

static bool ContainsMessageFragment(const TArray<FString>& Messages, const TCHAR* Fragment)
{
	for (const FString& Message : Messages)
	{
		if (Message.Contains(Fragment))
		{
			return true;
		}
	}

	return false;
}

static bool ContainsDiagnosticFragment(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics, const TCHAR* Fragment)
{
	for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
	{
		if (Diagnostic.bIsError && Diagnostic.Message.Contains(Fragment))
		{
			return true;
		}
	}

	return false;
}

static TSharedRef<FAngelscriptModuleDesc> MakeInMemoryModule(
	const FString& ModuleName,
	const FString& Filename,
	const FString& Source)
{
	TSharedRef<FAngelscriptModuleDesc> Module =
		MakeShared<FAngelscriptModuleDesc>();
	Module->ModuleName = ModuleName;
	FAngelscriptModuleDesc::FCodeSection& Section =
		Module->Code.AddDefaulted_GetRef();
	Section.RelativeFilename = FPaths::Combine(TEXT("Automation"), Filename);
	Section.AbsoluteFilename = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		Filename);
	Section.Code = Source;
	Section.CodeHash = static_cast<int64>(FCrc::StrCrc32(*Section.Code));
	Module->CodeHash = Section.CodeHash;
	return Module;
}

class FRecordingJITCompiler final : public asIJITCompiler
{
public:
	void OnFunctionReady(asIScriptFunction*) override
	{
		++OnFunctionReadyCount;
	}

	void ReleaseFunctionBinding(
		asIScriptFunction*,
		const asSJITFunctionBinding&) override
	{
	}

	int32 OnFunctionReadyCount = 0;
};

public:
	TEST_METHOD(RuntimeCompileRunsObservableBuilderStages)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("BuilderIntegrationRuntimeStages"),
			TEXT("BuilderIntegrationRuntimeStages.as"),
			TEXT(R"AS(
int BuilderIntegrationAdd(int Delta)
{
	return 40 + Delta;
}

int Entry()
{
	return BuilderIntegrationAdd(2);
}
)AS"),
			false,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Builder integration compile should succeed")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("Builder integration compile should be fully handled")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Builder integration compile should produce one compiled module")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Builder integration compile should not emit diagnostics")));

		const FName ExpectedPhases[] = {
			TEXT("Compile.ModuleAssembly"),
			TEXT("Compile.ModuleParse"),
			TEXT("Compile.ModuleGenerateTypes"),
			TEXT("Compile.ModuleGenerateFunctions"),
			TEXT("Compile.ModuleLayout"),
			TEXT("Compile.ModuleCompileCode"),
			TEXT("Compile.ModuleGlobals"),
		};

		int32 PreviousIndex = INDEX_NONE;
		for (FName Phase : ExpectedPhases)
		{
			const int32 PhaseIndex = FindFirstPhaseIndex(Events, Phase);
			if (this->Assert.IsTrue(PhaseIndex != INDEX_NONE, FString::Printf(TEXT("Builder runtime phase should be emitted: %s"), *Phase.ToString())))
			{
				ASSERT_THAT(IsTrue(PhaseIndex > PreviousIndex, FString::Printf(TEXT("Builder runtime phase should be ordered: %s"), *Phase.ToString())));
				PreviousIndex = PhaseIndex;
			}
		}

		const FAngelscriptCompilationEvent* CodeEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileModuleCompileCode);
		if (this->Assert.IsNotNull(CodeEvent, TEXT("Builder integration compile should emit codegen event")))
		{
			ASSERT_THAT(IsTrue(CodeEvent->ModuleNames.Contains(TEXT("BuilderIntegrationRuntimeStages")), TEXT("Codegen event should carry module name")));
			ASSERT_THAT(IsFalse(CodeEvent->bLoadedPrecompiledCode, TEXT("Codegen event should report the builder path rather than precompiled bypass")));
			ASSERT_THAT(AreEqual(ECompileType::SoftReloadOnly, CodeEvent->CompileType, TEXT("Codegen event should carry compile type")));
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			TEXT("BuilderIntegrationRuntimeStages"),
			TEXT("int Entry()"),
			EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Builder integration compile should execute Entry through runtime engine")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(42, EntryResult, TEXT("Builder integration compile should execute builder-produced bytecode")));
		}

		}
	}

	TEST_METHOD(RuntimeCompileFailureReportsBuilderDiagnostics)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle = FAngelscriptCompilationEvents::RegisterListener(
			[&Events](const FAngelscriptCompilationEvent& Event)
			{
				Events.Add(Event);
			});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("BuilderIntegrationRuntimeFailure"),
			TEXT("BuilderIntegrationRuntimeFailure.as"),
			TEXT(R"AS(
int Entry()
{
	GhostBuilderType Value;
	return 42;
}
)AS"),
			false,
			Summary,
			true);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Builder integration invalid script should fail")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Builder integration invalid script should report error result")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Builder integration invalid script should report the attempted module count")));
		ASSERT_THAT(IsTrue(
			ContainsDiagnosticFragment(Summary.Diagnostics, TEXT("GhostBuilderType")),
			TEXT("Builder integration invalid script should capture unknown-type diagnostic")));

		const FAngelscriptCompilationEvent* EndEvent = FindFirstEvent(Events, EAngelscriptCompilationEventType::CompileEnd);
		if (this->Assert.IsNotNull(EndEvent, TEXT("Builder integration invalid script should emit Compile.End")))
		{
			ASSERT_THAT(IsTrue(EndEvent->bFailed, TEXT("Builder integration invalid script should mark end event failed")));
			ASSERT_THAT(IsFalse(EndEvent->bSucceeded, TEXT("Builder integration invalid script should not mark end event succeeded")));
			ASSERT_THAT(AreEqual(ECompileResult::Error, EndEvent->CompileResult, TEXT("Builder integration invalid script should carry error result")));
			ASSERT_THAT(IsTrue(
				EndEvent->DiagnosticCount > 0 || ContainsMessageFragment(EndEvent->Messages, TEXT("GhostBuilderType")),
				TEXT("Builder integration invalid script should expose failure diagnostics on end event")));
		}

		if (TSharedPtr<FAngelscriptModuleDesc> FailedModule = Engine.GetModule(TEXT("BuilderIntegrationRuntimeFailure")))
		{
			ASSERT_THAT(IsNull(
				FailedModule->ScriptModule,
				TEXT("Builder integration invalid script should not leave executable script module state behind")));
		}

		}
	}

	TEST_METHOD(CanonicalStage2FailureAbandonsCandidateBeforeLayoutAndAllowsRetry)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Canonical Stage 2 lifecycle test should retain its script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline OriginalPipeline =
			ScriptEngine->GetCompilerPipeline();
		ASSERT_THAT(IsTrue(
			ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("Canonical Stage 2 lifecycle test should select the CANONICAL pipeline")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(OriginalPipeline);
		};

		const FName ModuleName(TEXT("CanonicalStage2FailureLifecycle"));
		const FString ModuleNameString = ModuleName.ToString();
		const FString Filename(TEXT("CanonicalStage2FailureLifecycle.as"));
		const FString InitialSource = ASTEST_AS(R"AS(
			class FStage2PublishedType
			{
				int Value;
			}

			int Entry()
			{
				return 11;
			}
			)AS");
		const FString CandidateSource = ASTEST_AS(R"AS(
			class FStage2AbortProbe
			{
				int Value;
			}

			const int Stage2AbortGlobal = 12;
			const FString Stage2AbortStorage = "candidate";

			int Entry()
			{
				return Stage2AbortGlobal;
			}
			)AS");
		const FString FixedSource = ASTEST_AS(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");

		FAngelscriptCompileTraceSummary InitialSummary;
		const bool bInitialCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			Filename,
			InitialSource,
			false,
			InitialSummary);
		ASSERT_THAT(IsTrue(
			bInitialCompiled,
			TEXT("Canonical Stage 2 lifecycle test should publish its initial generation")));
		if (!bInitialCompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> InitialModule =
			Engine.GetModule(ModuleNameString);
		ASSERT_THAT(IsNotNull(
			InitialModule.Get(),
			TEXT("Canonical Stage 2 lifecycle test should retain the initial module descriptor")));
		if (!InitialModule.IsValid())
		{
			return;
		}
		asIScriptModule* const InitialScriptModule = InitialModule->ScriptModule;
		ASSERT_THAT(IsNotNull(
			InitialScriptModule,
			TEXT("Canonical Stage 2 lifecycle test should retain the initial executable module")));
		asITypeInfo* const InitialPublishedType = InitialScriptModule != nullptr
			? InitialScriptModule->GetTypeInfoByDecl("FStage2PublishedType")
			: nullptr;
		asIScriptFunction* const InitialEntryFunction = InitialScriptModule != nullptr
			? InitialScriptModule->GetFunctionByDecl("int Entry()")
			: nullptr;
		ASSERT_THAT(IsNotNull(
			InitialPublishedType,
			TEXT("Canonical Stage 2 lifecycle test should publish its initial script type")));
		ASSERT_THAT(IsNotNull(
			InitialEntryFunction,
			TEXT("Canonical Stage 2 lifecycle test should publish its initial global function")));
		auto FindEngineVisiblePublishedType = [ScriptEngine]() -> asCTypeInfo*
		{
			asCTypeInfo* VisibleType = nullptr;
			ScriptEngine->allScriptDeclaredTypes.FindAllUntil(
				"FStage2PublishedType",
				[&VisibleType](asCTypeInfo* Candidate)
				{
					if (Candidate != nullptr
						&& Candidate->nameSpace != nullptr
						&& Candidate->nameSpace->name.GetLength() == 0)
					{
						VisibleType = Candidate;
						return true;
					}
					return false;
				});
			return VisibleType;
		};
		ASSERT_THAT(IsTrue(
			FindEngineVisiblePublishedType() == InitialPublishedType,
			TEXT("The accepted initial generation should own Engine type lookup")));
		ASSERT_THAT(IsTrue(
			InitialEntryFunction != nullptr
				&& ScriptEngine->allScriptGlobalFunctions.Contains(
					static_cast<asCScriptFunction*>(InitialEntryFunction)),
			TEXT("The accepted initial generation should own automatic-import function lookup")));

		int32 InitialResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				&Engine,
				ModuleName,
				TEXT("int Entry()"),
				InitialResult),
			TEXT("Canonical Stage 2 lifecycle test should execute the initial generation")));
		ASSERT_THAT(AreEqual(
			11,
			InitialResult,
			TEXT("Canonical Stage 2 lifecycle test should observe the initial result")));

		const asPWORD OriginalIncludeJIT = ScriptEngine->GetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS);
		asIJITCompiler* const OriginalJITCompiler =
			ScriptEngine->GetJITCompiler();
		FRecordingJITCompiler RecordingJITCompiler;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("Canonical Stage 2 lifecycle test should enable observable JIT entries")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&RecordingJITCompiler),
			TEXT("Canonical Stage 2 lifecycle test should install its recording JIT compiler")));
		// SetJITCompiler replays already-live functions into the new compiler.
		// The lifecycle gate measures only notifications produced by the candidate.
		RecordingJITCompiler.OnFunctionReadyCount = 0;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(OriginalJITCompiler);
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				OriginalIncludeJIT);
		};

		TSharedRef<FAngelscriptModuleDesc> CandidateModule =
			MakeShared<FAngelscriptModuleDesc>();
		CandidateModule->ModuleName = ModuleNameString;
		FAngelscriptModuleDesc::FCodeSection& CandidateSection =
			CandidateModule->Code.AddDefaulted_GetRef();
		CandidateSection.RelativeFilename =
			FPaths::Combine(TEXT("Automation"), Filename);
		CandidateSection.AbsoluteFilename = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			Filename);
		CandidateSection.Code = CandidateSource;
		CandidateSection.CodeHash = static_cast<int64>(
			FCrc::StrCrc32(*CandidateSection.Code));
		CandidateModule->CodeHash = CandidateSection.CodeHash;

		TArray<FAngelscriptCompilationEvent> Events;
		bool bInjectedPostStage2Failure = false;
		bool bObservedSuccessfulStage2 = false;
		bool bObservedFailedLayoutMilestone = false;
		bool bObservedFailedCodeMilestone = false;
		bool bClassLayoutedAfterFailure = false;
		bool bDestructorEvaluatedAfterFailure = false;
		bool bGlobalAllocatedAfterFailure = false;
		bool bBuilderReleasedBeforeCodeMilestone = false;
		const FDelegateHandle ListenerHandle =
			FAngelscriptCompilationEvents::RegisterListener(
				[&](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
					if (!Event.ModuleNames.Contains(ModuleNameString))
					{
						return;
					}

					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleGenerateFunctions
						&& !bInjectedPostStage2Failure)
					{
						bObservedSuccessfulStage2 =
							Event.bSucceeded && !Event.bFailed;
						CandidateModule->bCompileError = true;
						bInjectedPostStage2Failure = true;
						return;
					}

					asCModule* const ScriptCandidate = static_cast<asCModule*>(
						CandidateModule->ScriptModule);
					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleLayout)
					{
						bObservedFailedLayoutMilestone =
							Event.bFailed && !Event.bSucceeded;
						if (ScriptCandidate == nullptr
							|| ScriptCandidate->builder == nullptr)
						{
							return;
						}
						for (asUINT Index = 0;
							Index < ScriptCandidate->builder->classDeclarations.GetLength();
							++Index)
						{
							const sClassDeclaration* const Declaration =
								ScriptCandidate->builder->classDeclarations[Index];
							if (Declaration == nullptr)
							{
								continue;
							}
							bClassLayoutedAfterFailure =
								bClassLayoutedAfterFailure || Declaration->hasLayouted;
							bDestructorEvaluatedAfterFailure =
								bDestructorEvaluatedAfterFailure
								|| Declaration->evaluatedDestructor;
						}
						for (asUINT Index = 0;
							Index < ScriptCandidate->builder->globVariableList.GetLength();
							++Index)
						{
							const sGlobalVariableDescription* const Global =
								ScriptCandidate->builder->globVariableList[Index];
							bGlobalAllocatedAfterFailure =
								bGlobalAllocatedAfterFailure
								|| (Global != nullptr
									&& Global->property != nullptr
									&& Global->property->memoryAllocated);
						}
						return;
					}

					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleCompileCode)
					{
						bObservedFailedCodeMilestone =
							Event.bFailed && !Event.bSucceeded
							&& !Event.bJitHandoff;
						bBuilderReleasedBeforeCodeMilestone =
							ScriptCandidate != nullptr
							&& ScriptCandidate->builder == nullptr;
					}
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		TArray<TSharedRef<FAngelscriptModuleDesc>> CandidateModules;
		CandidateModules.Add(CandidateModule);
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledCandidates;
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod,
			false);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult BrokenCompileResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			CandidateModules,
			CompiledCandidates);
		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			BrokenCompileResult,
			TEXT("Canonical Stage 2 lifecycle test should report a failed candidate")));
		ASSERT_THAT(IsTrue(
			bInjectedPostStage2Failure,
			TEXT("Canonical Stage 2 lifecycle test should inject failure after Stage 2")));
		ASSERT_THAT(IsTrue(
			bObservedSuccessfulStage2,
			TEXT("Canonical Stage 2 lifecycle test should inject only after a successful Stage 2")));

		const FAngelscriptCompilationEvent* GenerateFunctionsEvent =
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleGenerateFunctions,
				ModuleNameString);
		ASSERT_THAT(IsNotNull(
			GenerateFunctionsEvent,
			TEXT("Canonical Stage 2 lifecycle test should reach GenerateFunctions")));
		if (GenerateFunctionsEvent != nullptr)
		{
			ASSERT_THAT(IsTrue(
				GenerateFunctionsEvent->bSucceeded,
				TEXT("The injected failure must occur after successful GenerateFunctions")));
		}

		ASSERT_THAT(IsTrue(
			bObservedFailedLayoutMilestone,
			TEXT("A post-Stage 2 failure should retain a failed layout milestone")));
		const bool bSkippedClassLayout = this->Assert.IsFalse(
			bClassLayoutedAfterFailure,
			TEXT("A post-Stage 2 failure must not layout candidate classes"));
		const bool bSkippedDestructorSynthesis = this->Assert.IsFalse(
			bDestructorEvaluatedAfterFailure,
			TEXT("A post-Stage 2 failure must not synthesize candidate destructors"));
		const bool bSkippedGlobalAllocation = this->Assert.IsFalse(
			bGlobalAllocatedAfterFailure,
			TEXT("A post-Stage 2 failure must not allocate candidate globals"));
		ASSERT_THAT(IsTrue(
			bObservedFailedCodeMilestone,
			TEXT("A post-Stage 2 failure should retain a failed code milestone without JIT handoff")));
		const bool bReleasedBuilder = this->Assert.IsTrue(
			bBuilderReleasedBeforeCodeMilestone,
			TEXT("A failed code milestone should observe deterministic Builder release"));
		const bool bSkippedJIT = this->Assert.IsTrue(
			RecordingJITCompiler.OnFunctionReadyCount == 0,
			TEXT("A post-Stage 2 failure must not notify the JIT compiler"));
		const bool bDirectPerModuleAbandonWasComplete =
			bSkippedClassLayout
			&& bSkippedDestructorSynthesis
			&& bSkippedGlobalAllocation
			&& bReleasedBuilder
			&& bSkippedJIT;
		ASSERT_THAT(IsNull(
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleGlobals,
				ModuleNameString),
			TEXT("A Stage 2 failure must not initialize candidate globals")));
		ASSERT_THAT(IsNull(
			CandidateModule->ScriptModule,
			TEXT("The failed candidate descriptor should release its discarded module")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleAfterFailure =
			Engine.GetModule(ModuleNameString);
		ASSERT_THAT(IsNotNull(
			ModuleAfterFailure.Get(),
			TEXT("A failed candidate should keep the last-good module descriptor active")));
		if (ModuleAfterFailure.IsValid())
		{
			ASSERT_THAT(IsTrue(
				ModuleAfterFailure->ScriptModule == InitialScriptModule,
				TEXT("A Stage 2 failure must not replace the active executable generation")));
		}
		ASSERT_THAT(IsTrue(
			InitialScriptModule != nullptr
				&& InitialScriptModule->GetTypeInfoByName("FStage2PublishedType")
					== InitialPublishedType,
			TEXT("The last-good module should retain its local type inventory after rollback")));
		ASSERT_THAT(IsTrue(
			FindEngineVisiblePublishedType() == InitialPublishedType,
			TEXT("A failed staged replacement must restore the last-good type to Engine lookup")));
		ASSERT_THAT(IsTrue(
			InitialEntryFunction != nullptr
				&& ScriptEngine->allScriptGlobalFunctions.Contains(
					static_cast<asCScriptFunction*>(InitialEntryFunction)),
			TEXT("A failed staged replacement must restore the last-good function to automatic-import lookup")));

		int32 ResultAfterFailure = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				&Engine,
				ModuleName,
				TEXT("int Entry()"),
				ResultAfterFailure),
			TEXT("The last-good generation should remain executable after Stage 2 failure")));
		ASSERT_THAT(AreEqual(
			11,
			ResultAfterFailure,
			TEXT("Stage 2 failure should preserve the last-good runtime result")));

		FAngelscriptCompileTraceSummary FixedSummary;
		const bool bFixedCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			Filename,
			FixedSource,
			false,
			FixedSummary);
		ASSERT_THAT(IsTrue(
			bFixedCompiled,
			TEXT("Canonical Stage 2 lifecycle test should allow a corrected same-name retry")));
		if (!bFixedCompiled)
		{
			return;
		}

		int32 FixedResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				&Engine,
				ModuleName,
				TEXT("int Entry()"),
				FixedResult),
			TEXT("Canonical Stage 2 lifecycle test should execute the corrected generation")));
		ASSERT_THAT(AreEqual(
			42,
			FixedResult,
			TEXT("Canonical Stage 2 lifecycle test should publish the corrected result")));
		ASSERT_THAT(IsTrue(
			bDirectPerModuleAbandonWasComplete,
			TEXT("Canonical Stage 2 failure should abandon every covered direct per-module route")));

		}
	}

	TEST_METHOD(CanonicalRejectedMutableGlobalSkipsAllocationAndAllowsRetry)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Mutable-global rejection test should retain its script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline OriginalPipeline =
			ScriptEngine->GetCompilerPipeline();
		ASSERT_THAT(IsTrue(
			ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("Mutable-global rejection test should select the CANONICAL pipeline")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(OriginalPipeline);
		};

		const FName ModuleName(TEXT("CanonicalRejectedMutableGlobal"));
		const FString ModuleNameString = ModuleName.ToString();
		const FString Filename(TEXT("CanonicalRejectedMutableGlobal.as"));
		const FString BrokenSource = ASTEST_AS(R"AS(
			int RejectedMutableGlobal = 41;

			int Entry()
			{
				return RejectedMutableGlobal + 1;
			}
			)AS");
		const FString FixedSource = ASTEST_AS(R"AS(
			const int AcceptedConstantGlobal = 41;

			int Entry()
			{
				return AcceptedConstantGlobal + 1;
			}
			)AS");

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle ListenerHandle =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		FAngelscriptCompileTraceSummary BrokenSummary;
		const bool bBrokenCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			Filename,
			BrokenSource,
			false,
			BrokenSummary,
			true);
		ASSERT_THAT(IsFalse(
			bBrokenCompiled,
			TEXT("CANONICAL should reject mutable globals without entering allocation")));
		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			BrokenSummary.CompileResult,
			TEXT("Mutable-global rejection should surface a compile error")));
		ASSERT_THAT(IsTrue(
			ContainsDiagnosticFragment(BrokenSummary.Diagnostics, TEXT("must be const")),
			TEXT("Mutable-global rejection should retain its product diagnostic")));

		const FAngelscriptCompilationEvent* GenerateFunctionsEvent =
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleGenerateFunctions,
				ModuleNameString);
		ASSERT_THAT(IsNotNull(
			GenerateFunctionsEvent,
			TEXT("Mutable-global rejection should reach its Stage 2 milestone")));
		if (GenerateFunctionsEvent != nullptr)
		{
			ASSERT_THAT(IsTrue(
				GenerateFunctionsEvent->bFailed,
				TEXT("Mutable-global rejection should fail at Stage 2")));
		}

		const FAngelscriptCompilationEvent* LayoutEvent =
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleLayout,
				ModuleNameString);
		ASSERT_THAT(IsNotNull(
			LayoutEvent,
			TEXT("Mutable-global rejection should retain a failed layout milestone")));
		if (LayoutEvent != nullptr)
		{
			ASSERT_THAT(IsTrue(
				LayoutEvent->bFailed,
				TEXT("Mutable-global rejection should not report layout success")));
		}

		const FAngelscriptCompilationEvent* CodeEvent =
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleCompileCode,
				ModuleNameString);
		ASSERT_THAT(IsNotNull(
			CodeEvent,
			TEXT("Mutable-global rejection should retain a failed code milestone")));
		if (CodeEvent != nullptr)
		{
			ASSERT_THAT(IsTrue(
				CodeEvent->bFailed && !CodeEvent->bJitHandoff,
				TEXT("Mutable-global rejection should not enter CodeGen or JIT")));
		}
		ASSERT_THAT(IsNull(
			FindFirstEventForModule(
				Events,
				EAngelscriptCompilationEventType::CompileModuleGlobals,
				ModuleNameString),
			TEXT("Mutable-global rejection must not initialize candidate globals")));

		if (TSharedPtr<FAngelscriptModuleDesc> FailedModule =
			Engine.GetModule(ModuleNameString))
		{
			ASSERT_THAT(IsNull(
				FailedModule->ScriptModule,
				TEXT("Mutable-global rejection should not publish an executable module")));
		}

		FAngelscriptCompileTraceSummary FixedSummary;
		const bool bFixedCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			Filename,
			FixedSource,
			false,
			FixedSummary);
		ASSERT_THAT(IsTrue(
			bFixedCompiled,
			TEXT("A valid same-name compile should recover after mutable-global rejection")));
		if (!bFixedCompiled)
		{
			return;
		}

		int32 FixedResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(
				&Engine,
				ModuleName,
				TEXT("int Entry()"),
				FixedResult),
			TEXT("Recovered mutable-global module should execute")));
		ASSERT_THAT(AreEqual(
			42,
			FixedResult,
			TEXT("Recovered mutable-global module should publish the fixed result")));

		}
	}

	TEST_METHOD(CanonicalMixedBatchSkipsReferenceReplacementOnFailedCandidate)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Mixed-batch replacement test should retain its script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline OriginalPipeline =
			ScriptEngine->GetCompilerPipeline();
		const asPWORD OriginalAutomaticImports = ScriptEngine->GetEngineProperty(
			asEP_AUTOMATIC_IMPORTS);
		ASSERT_THAT(IsTrue(
			ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("Mixed-batch replacement test should select CANONICAL")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 1),
			TEXT("Mixed-batch replacement test should enable automatic imports")));
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod,
			true);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(
				asEP_AUTOMATIC_IMPORTS,
				OriginalAutomaticImports);
			ScriptEngine->SetCompilerPipeline(OriginalPipeline);
		};

		const FString ModuleAName(TEXT("CanonicalMixedBatchA"));
		const FString ModuleBName(TEXT("CanonicalMixedBatchB"));
		const FString SourceA1 = ASTEST_AS(R"AS(
			class FCanonicalMixedA
			{
				int Value;
			}

			namespace CanonicalMixedAApi
			{
				int Value()
				{
					return 10;
				}
			}
			)AS");
		const FString SourceB1 = ASTEST_AS(R"AS(
			class FCanonicalMixedB
			{
				int Value;
			}

			namespace CanonicalMixedBApi
			{
				int Value()
				{
					return CanonicalMixedAApi::Value() + 10;
				}
			}
			)AS");
		const FString SourceA2 = ASTEST_AS(R"AS(
			class FCanonicalMixedA
			{
				int Value;
				int Added;
			}

			namespace CanonicalMixedAApi
			{
				int Value()
				{
					return 11;
				}
			}
			)AS");

		auto CompileSingle = [&](const FString& ModuleName,
			const FString& Filename,
			const FString& Source) -> ECompileResult
		{
			TArray<TSharedRef<FAngelscriptModuleDesc>> Inputs;
			Inputs.Add(MakeInMemoryModule(ModuleName, Filename, Source));
			TArray<TSharedRef<FAngelscriptModuleDesc>> Outputs;
			return Engine.CompileModules(
				ECompileType::SoftReloadOnly,
				Inputs,
				Outputs);
		};

		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			CompileSingle(ModuleAName, TEXT("CanonicalMixedBatchA.as"), SourceA1),
			TEXT("Mixed-batch replacement test should publish generation A1")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			CompileSingle(ModuleBName, TEXT("CanonicalMixedBatchB.as"), SourceB1),
			TEXT("Mixed-batch replacement test should publish generation B1")));

		TSharedPtr<FAngelscriptModuleDesc> OldModuleA = Engine.GetModule(ModuleAName);
		TSharedPtr<FAngelscriptModuleDesc> OldModuleB = Engine.GetModule(ModuleBName);
		ASSERT_THAT(IsNotNull(
			OldModuleA.Get(),
			TEXT("Mixed-batch replacement test should retain generation A1")));
		ASSERT_THAT(IsNotNull(
			OldModuleB.Get(),
			TEXT("Mixed-batch replacement test should retain generation B1")));
		if (!OldModuleA.IsValid() || !OldModuleB.IsValid())
		{
			return;
		}

		asCModule* const OldScriptModuleA = static_cast<asCModule*>(
			OldModuleA->ScriptModule);
		asCModule* const OldScriptModuleB = static_cast<asCModule*>(
			OldModuleB->ScriptModule);
		ASSERT_THAT(IsNotNull(
			OldScriptModuleA,
			TEXT("Mixed-batch replacement test should retain executable A1")));
		ASSERT_THAT(IsNotNull(
			OldScriptModuleB,
			TEXT("Mixed-batch replacement test should retain executable B1")));
		if (OldScriptModuleA == nullptr
			|| OldScriptModuleB == nullptr)
		{
			return;
		}

		TSharedRef<FAngelscriptModuleDesc> CandidateA = MakeInMemoryModule(
			ModuleAName,
			TEXT("CanonicalMixedBatchA.as"),
			SourceA2);
		bool bObservedDependencyStage2 = false;
		bool bInjectedOldBDependency = false;
		bool bInjectedCandidateBLayoutFailure = false;
		bool bInjectedFailureAfterDependencyStage2 = false;
		bool bObservedFailedLayout = false;
		bool bObservedOldBDependencyBeforeInjection = false;
		bool bRetainedOldBDependencyAtFailedLayout = false;
		const FDelegateHandle ListenerHandle =
			FAngelscriptCompilationEvents::RegisterListener(
				[&](const FAngelscriptCompilationEvent& Event)
				{
					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleGenerateFunctions
						&& Event.ModuleNames.Contains(ModuleAName)
						&& !bInjectedOldBDependency)
					{
						asCModule* const CandidateScriptModule =
							static_cast<asCModule*>(CandidateA->ScriptModule);
						asCScriptFunction* const OldBFunction =
							static_cast<asCScriptFunction*>(
								OldScriptModuleB->GetFunctionByDecl(
									"int CanonicalMixedBApi::Value()"));
						if (Event.bSucceeded && !Event.bFailed
							&& CandidateScriptModule != nullptr
							&& CandidateScriptModule->builder != nullptr
							&& OldBFunction != nullptr)
						{
							CandidateScriptModule->builder->MarkDependency(
								OldBFunction,
								nullptr,
								nullptr);
							bInjectedOldBDependency =
								CandidateScriptModule->moduleDependencies.Contains(
									OldScriptModuleB);
						}
						return;
					}

					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleGenerateFunctions
						&& Event.ModuleNames.Contains(ModuleBName)
						&& !bInjectedFailureAfterDependencyStage2)
					{
						bObservedDependencyStage2 =
							Event.bSucceeded && !Event.bFailed;
						asCModule* const CandidateScriptModule =
							static_cast<asCModule*>(CandidateA->ScriptModule);
						bObservedOldBDependencyBeforeInjection =
							CandidateScriptModule != nullptr
							&& CandidateScriptModule->moduleDependencies.Contains(
								OldScriptModuleB);
						asCModule* CandidateScriptModuleB = nullptr;
						for (asUINT ModuleIndex = 0;
							ModuleIndex < ScriptEngine->GetModuleCount();
							++ModuleIndex)
						{
							asCModule* const Candidate = static_cast<asCModule*>(
								ScriptEngine->GetModuleByIndex(ModuleIndex));
							if (Candidate != nullptr
								&& Candidate != OldScriptModuleB
								&& Candidate->baseModuleName
									== TCHAR_TO_ANSI(*ModuleBName)
								&& Candidate->builder != nullptr)
							{
								CandidateScriptModuleB = Candidate;
								break;
							}
						}
						if (CandidateScriptModuleB != nullptr
							&& CandidateScriptModuleB->builder != nullptr)
						{
							++CandidateScriptModuleB->builder->numErrors;
							bInjectedCandidateBLayoutFailure = true;
						}
						CandidateA->bCompileError = true;
						bInjectedFailureAfterDependencyStage2 = true;
						return;
					}

					if (Event.Type != EAngelscriptCompilationEventType::
						CompileModuleLayout
						|| !Event.ModuleNames.Contains(ModuleAName))
					{
						return;
					}
					bObservedFailedLayout = Event.bFailed && !Event.bSucceeded;
					asCModule* const FailedCandidate = static_cast<asCModule*>(
						CandidateA->ScriptModule);
					bRetainedOldBDependencyAtFailedLayout =
						FailedCandidate != nullptr
						&& FailedCandidate->moduleDependencies.Contains(
							OldScriptModuleB);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		TArray<TSharedRef<FAngelscriptModuleDesc>> CandidateInputs;
		CandidateInputs.Add(CandidateA);
		TArray<TSharedRef<FAngelscriptModuleDesc>> CandidateOutputs;
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult CandidateResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			CandidateInputs,
			CandidateOutputs);
		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			CandidateResult,
			TEXT("Mixed-batch replacement test should reject the injected batch")));
		ASSERT_THAT(IsTrue(
			bInjectedOldBDependency,
			TEXT("The Stage 2 seam should establish an exact A2-to-B1 dependency")));
		ASSERT_THAT(IsTrue(
			bObservedDependencyStage2,
			TEXT("A structural A2 change should automatically compile dependent B2")));
		ASSERT_THAT(IsTrue(
			bInjectedFailureAfterDependencyStage2,
			TEXT("Mixed-batch replacement test should inject only after B2 Stage 2")));
		ASSERT_THAT(IsTrue(
			bInjectedCandidateBLayoutFailure,
			TEXT("The Stage 2 seam should stop B2 before unrelated CodeGen")));
		ASSERT_THAT(IsTrue(
			bObservedOldBDependencyBeforeInjection,
			TEXT("A2 should retain its exact dependency on active generation B1 before batch replacement")));
		ASSERT_THAT(IsTrue(
			bObservedFailedLayout,
			TEXT("The rejected A2 candidate should retain a failed layout milestone")));
		ASSERT_THAT(IsTrue(
			bRetainedOldBDependencyAtFailedLayout,
			TEXT("A known-failed candidate must not receive broad dependency replacement from B1 to B2")));
		ASSERT_THAT(IsTrue(
			Engine.GetModule(ModuleAName)->ScriptModule == OldScriptModuleA,
			TEXT("Rejected mixed batch should keep generation A1 active")));
		ASSERT_THAT(IsTrue(
			Engine.GetModule(ModuleBName)->ScriptModule == OldScriptModuleB,
			TEXT("Rejected mixed batch should keep generation B1 active")));

		}
	}

	TEST_METHOD(CanonicalMixedBatchFailedTemplateSkipsDeferredWorkAndDrainsQueue)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Mixed-template batch test should retain its script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asECompilerPipeline OriginalPipeline =
			ScriptEngine->GetCompilerPipeline();
		ASSERT_THAT(IsTrue(
			ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("Mixed-template batch test should select CANONICAL")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetCompilerPipeline(OriginalPipeline);
		};

		const FString HealthyModuleName(TEXT("CanonicalHealthyTemplateBatch"));
		const FString FailedModuleName(TEXT("CanonicalFailedTemplateBatch"));
		TSharedRef<FAngelscriptModuleDesc> HealthyCandidate = MakeInMemoryModule(
			HealthyModuleName,
			TEXT("CanonicalHealthyTemplateBatch.as"),
			ASTEST_AS(R"AS(
				class FHealthyTemplatePayload
				{
					int Value;
				}

				int HealthyTemplateEntry()
				{
					return 21;
				}
				)AS"));
		TSharedRef<FAngelscriptModuleDesc> FailedCandidate = MakeInMemoryModule(
			FailedModuleName,
			TEXT("CanonicalFailedTemplateBatch.as"),
			ASTEST_AS(R"AS(
				class FFailedTemplatePayload
				{
					int Value;
				}

				int FailedTemplateEntry()
				{
					return 22;
				}
				)AS"));

		asCObjectType* const OptionalTemplate = static_cast<asCObjectType*>(
			ScriptEngine->GetTypeInfoByName("TOptional"));
		ASSERT_THAT(IsNotNull(
			OptionalTemplate,
			TEXT("The Runtime fixture should expose the real TOptional template")));
		if (OptionalTemplate == nullptr)
		{
			return;
		}
		auto CreateOptionalInstance = [ScriptEngine, OptionalTemplate](
			asCModule* RequestingModule,
			const char* PayloadName) -> asCObjectType*
		{
			asCTypeInfo* const Payload = RequestingModule != nullptr
				? static_cast<asCTypeInfo*>(
					RequestingModule->GetTypeInfoByName(PayloadName))
				: nullptr;
			if (Payload == nullptr)
			{
				return nullptr;
			}
			asCArray<asCDataType> SubTypes;
			SubTypes.PushLast(asCDataType::CreateType(Payload, false));
			return ScriptEngine->GetTemplateInstanceType(
				OptionalTemplate,
				SubTypes,
				RequestingModule,
				nullptr);
		};

		asCObjectType* HealthyInstance = nullptr;
		asCObjectType* FailedInstance = nullptr;
		bool bHealthyInstanceRetained = false;
		bool bFailedInstanceRetained = false;
		ON_SCOPE_EXIT
		{
			if (HealthyInstance != nullptr && bHealthyInstanceRetained)
			{
				HealthyInstance->Release();
			}
			if (FailedInstance != nullptr && bFailedInstanceRetained)
			{
				FailedInstance->Release();
			}
		};

		bool bHealthyDeferredPrecondition = false;
		bool bFailedDeferredPrecondition = false;
		bool bInjectedFailedCandidate = false;
		bool bHealthySizeCalculated = false;
		bool bFailedSizeLeftDeferred = false;
		const FDelegateHandle ListenerHandle =
			FAngelscriptCompilationEvents::RegisterListener(
				[&](const FAngelscriptCompilationEvent& Event)
				{
					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleGenerateFunctions
						&& Event.ModuleNames.Contains(HealthyModuleName)
						&& HealthyInstance == nullptr)
					{
						HealthyInstance = CreateOptionalInstance(
							static_cast<asCModule*>(HealthyCandidate->ScriptModule),
							"FHealthyTemplatePayload");
						if (HealthyInstance != nullptr)
						{
							HealthyInstance->AddRef();
							bHealthyInstanceRetained = true;
							bHealthyDeferredPrecondition =
								Event.bSucceeded && !Event.bFailed
								&& ScriptEngine->unvalidatedTemplateInstances.Contains(
									HealthyInstance)
								&& (HealthyInstance->flags
									& asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) != 0
								&& HealthyInstance->plainUserData == 0;
						}
						return;
					}

					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleGenerateFunctions
						&& Event.ModuleNames.Contains(FailedModuleName)
						&& FailedInstance == nullptr)
					{
						FailedInstance = CreateOptionalInstance(
							static_cast<asCModule*>(FailedCandidate->ScriptModule),
							"FFailedTemplatePayload");
						if (FailedInstance != nullptr)
						{
							FailedInstance->AddRef();
							bFailedInstanceRetained = true;
							bFailedDeferredPrecondition =
								Event.bSucceeded && !Event.bFailed
								&& ScriptEngine->unvalidatedTemplateInstances.Contains(
									FailedInstance)
								&& (FailedInstance->flags
									& asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) != 0
								&& FailedInstance->plainUserData == 0;
							FailedCandidate->bCompileError = true;
							bInjectedFailedCandidate = true;
						}
						return;
					}

					if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleLayout
						&& Event.ModuleNames.Contains(HealthyModuleName))
					{
						bHealthySizeCalculated = HealthyInstance != nullptr
							&& (HealthyInstance->flags
								& asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) == 0;
					}
					else if (Event.Type == EAngelscriptCompilationEventType::
						CompileModuleLayout
						&& Event.ModuleNames.Contains(FailedModuleName))
					{
						bFailedSizeLeftDeferred = FailedInstance != nullptr
							&& (FailedInstance->flags
								& asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) != 0;
					}
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(ListenerHandle);
		};

		TArray<TSharedRef<FAngelscriptModuleDesc>> CandidateInputs;
		CandidateInputs.Add(HealthyCandidate);
		CandidateInputs.Add(FailedCandidate);
		TArray<TSharedRef<FAngelscriptModuleDesc>> CandidateOutputs;
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const ECompileResult CandidateResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			CandidateInputs,
			CandidateOutputs);

		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			CandidateResult,
			TEXT("The mixed-template batch should reject the injected candidate")));
		ASSERT_THAT(IsTrue(
			bHealthyDeferredPrecondition,
			TEXT("The healthy TOptional instance should begin in the deferred queue without size or callback work")));
		ASSERT_THAT(IsTrue(
			bFailedDeferredPrecondition,
			TEXT("The failed-only TOptional instance should begin in the deferred queue without size or callback work")));
		ASSERT_THAT(IsTrue(
			bInjectedFailedCandidate,
			TEXT("The template test should inject failure after the failed candidate creates its instance")));
		ASSERT_THAT(IsTrue(
			bHealthySizeCalculated,
			TEXT("A healthy requestor must still receive deferred template size work")));
		ASSERT_THAT(IsTrue(
			bFailedSizeLeftDeferred,
			TEXT("A failed-only requestor must not execute deferred template size work")));
		ASSERT_THAT(IsTrue(
			HealthyInstance != nullptr && HealthyInstance->plainUserData != 0,
			TEXT("A healthy requestor must still execute the real TOptional validation callback")));
		ASSERT_THAT(IsTrue(
			FailedInstance != nullptr && FailedInstance->plainUserData == 0,
			TEXT("A failed-only requestor must not execute the real TOptional validation callback")));
		ASSERT_THAT(AreEqual(
			0,
			ScriptEngine->unvalidatedTemplateInstances.Num(),
			TEXT("A failed mixed batch must drain every deferred template queue entry")));

		}
	}
};

#endif
