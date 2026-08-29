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
};

#endif
