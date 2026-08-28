#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeExit.h"

#if AS_CAN_GENERATE_JIT
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#endif

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_public_view.h"
#include "source/as_ast_sidecar.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTSnapshotReloadTests,
	"Angelscript.TestModule.HotReload.CanonicalAST.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName SnapshotModuleName = FName(TEXT("CanonicalASTSnapshotReload"));
	inline static const FName FailModuleName = FName(TEXT("CanonicalASTSnapshotReloadFail"));
	inline static const FName ConcurrentModuleName = FName(TEXT("CanonicalASTSnapshotConcurrent"));

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CanonicalSoftReloadPublishesNewDigestWithoutLegacyCompiler)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("the real Hot Reload fixture needs the Engine-owned script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const int PreviousPipeline = ScriptEngine->GetCompilerPipeline();
		Engine.NextCompileASTRetentionPolicy =
			static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*SnapshotModuleName.ToString());
			ScriptEngine->SetCompilerPipeline(
				static_cast<asECompilerPipeline>(PreviousPipeline));
		};
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(
			asCOMPILER_PIPELINE_CANONICAL),
			TEXT("the Hot Reload gate must select CANONICAL for the whole Engine")));

		const FString SourceA = ASTEST_AS(R"AS(
			int CanonicalHotReloadEntry()
			{
				return 11;
			}

			UCLASS()
			class UCanonicalASTPublisherReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return CanonicalHotReloadEntry();
				}
			}
			)AS");
		const FString SourceB = ASTEST_AS(R"AS(
			int CanonicalHotReloadEntry()
			{
				return 29;
			}

			UCLASS()
			class UCanonicalASTPublisherReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return CanonicalHotReloadEntry();
				}
			}
			)AS");
		const FString SourceBad = ASTEST_AS(R"AS(
			int CanonicalHotReloadEntry()
			{
				unknown_symbol;
				return 41;
			}

			UCLASS()
			class UCanonicalASTPublisherReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return CanonicalHotReloadEntry();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileAnnotatedModuleFromMemory(
			&Engine,
			SnapshotModuleName,
			TEXT("CanonicalASTSnapshotReload.as"),
			SourceA),
			TEXT("canonical generation A should compile through the real staged host")));
		const TSharedPtr<FAngelscriptModuleDesc> RecordA =
			Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		asCModule* const ModuleA = RecordA.IsValid()
			? RecordA->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(ModuleA,
			TEXT("generation A must expose its prepared Runtime module")));
		if (ModuleA == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN),
			static_cast<int32>(ModuleA->GetLastBytecodePublisher()),
			TEXT("generation A must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ModuleA->GetLastLegacyCompilerInvocationCount(),
			TEXT("generation A must not invoke asCCompiler")));
		asQWORD DigestA = 0;
		ASSERT_THAT(IsTrue(ModuleA->GetLastCanonicalCodeGenAstDigest(DigestA),
			TEXT("generation A must publish the exact sealed-AST digest")));
		asIASTSnapshot* GenerationA =
			ModuleA->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA,
			TEXT("generation A must publish a retained snapshot")));
		if (GenerationA == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			GenerationA->Release();
		};

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			SnapshotModuleName,
			TEXT("CanonicalASTSnapshotReload.as"),
			SourceB,
			ReloadResult),
			TEXT("canonical generation B should compile through SoftReload")));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled
				|| ReloadResult == ECompileResult::PartiallyHandled,
			TEXT("generation B must be an applied Hot Reload transaction")));

		const TSharedPtr<FAngelscriptModuleDesc> RecordB =
			Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		asCModule* const ModuleB = RecordB.IsValid()
			? RecordB->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(ModuleB,
			TEXT("generation B must expose its replacement Runtime module")));
		if (ModuleB == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN),
			static_cast<int32>(ModuleB->GetLastBytecodePublisher()),
			TEXT("Hot Reload must publish Bytecode from generation B's sealed AST")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			ModuleB->GetLastLegacyCompilerInvocationCount(),
			TEXT("Hot Reload must not fall back to asCCompiler")));
		asQWORD DigestB = 0;
		ASSERT_THAT(IsTrue(ModuleB->GetLastCanonicalCodeGenAstDigest(DigestB),
			TEXT("generation B must publish the exact sealed-AST digest")));
		ASSERT_THAT(AreNotEqual(DigestA, DigestB,
			TEXT("the changed function body must produce a new canonical digest")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(),
			TEXT("successful Hot Reload must retire generation A without invalidating its lease")));
		asIASTSnapshot* GenerationB =
			ModuleB->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationB,
			TEXT("generation B must publish a retained snapshot")));
		if (GenerationB == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			GenerationB->Release();
		};
		ASSERT_THAT(IsTrue(GenerationB->IsCurrentGeneration(),
			TEXT("generation B snapshot must be current after commit")));
		const FString GenerationKeyB =
			UTF8_TO_TCHAR(GenerationB->GetGenerationKey());

		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			&Engine,
			SnapshotModuleName,
			TEXT("int CanonicalHotReloadEntry()"),
			Value)
			&& Value == 29,
			TEXT("the active Hot Reload generation must execute generation B's body")));

		TestRunner->AddExpectedError(
			TEXT("CanonicalASTSnapshotReload.as"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedError(
			TEXT("unknown_symbol"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			0);
		ECompileResult FailedReloadResult = ECompileResult::FullyHandled;
		ASSERT_THAT(IsFalse(CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			SnapshotModuleName,
			TEXT("CanonicalASTSnapshotReload.as"),
			SourceBad,
			FailedReloadResult),
			TEXT("the malformed generation C must be rejected")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ECompileResult::Error),
			static_cast<int32>(FailedReloadResult),
			TEXT("the rejected code-only replacement must report a compile error")));

		const TSharedPtr<FAngelscriptModuleDesc> AfterFailedRecord =
			Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		asCModule* const AfterFailedModule = AfterFailedRecord.IsValid()
			? AfterFailedRecord->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(AfterFailedModule,
			TEXT("failed Hot Reload must keep generation B's Runtime module")));
		if (AfterFailedModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN),
			static_cast<int32>(AfterFailedModule->GetLastBytecodePublisher()),
			TEXT("failed Hot Reload must retain the last canonical publisher")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0),
			AfterFailedModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("failed Hot Reload must not construct a legacy compiler fallback")));
		asQWORD DigestAfterFailure = 0;
		ASSERT_THAT(IsTrue(
			AfterFailedModule->GetLastCanonicalCodeGenAstDigest(
				DigestAfterFailure),
			TEXT("failed Hot Reload must retain generation B's digest")));
		ASSERT_THAT(AreEqual(DigestB, DigestAfterFailure,
			TEXT("failed Hot Reload must not publish generation C's AST digest")));
		asIASTSnapshot* KeptGeneration =
			AfterFailedModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(KeptGeneration,
			TEXT("failed Hot Reload must retain generation B's snapshot")));
		if (KeptGeneration != nullptr)
		{
			ASSERT_THAT(IsTrue(KeptGeneration->IsCurrentGeneration(),
				TEXT("the retained generation B snapshot must remain current")));
			ASSERT_THAT(AreEqual(
				GenerationKeyB,
				FString(UTF8_TO_TCHAR(KeptGeneration->GetGenerationKey())),
				TEXT("failed Hot Reload must not publish a new snapshot generation")));
			KeptGeneration->Release();
		}
		Value = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(
			&Engine,
			SnapshotModuleName,
			TEXT("int CanonicalHotReloadEntry()"),
			Value)
			&& Value == 29,
			TEXT("failed Hot Reload must leave generation B executable")));
	}

	TEST_METHOD(HotReloadPublishesNewGenerationAndKeepsOldLease)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*SnapshotModuleName.ToString());
		};

		const FString SourceA = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 1;
				}
			}
			)AS");
		const FString SourceB = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 2;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SnapshotModuleName, TEXT("CanonicalASTSnapshotReload.as"), SourceA),
			TEXT("generation A should compile")));
		const TSharedPtr<FAngelscriptModuleDesc> RecordA = Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		ASSERT_THAT(IsTrue(RecordA.IsValid() && RecordA->ScriptModule != nullptr,
			TEXT("generation A must expose ScriptModule")));
		asIASTSnapshot* GenerationA = RecordA->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA, TEXT("retain policy should publish generation A")));
		ASSERT_THAT(IsTrue(GenerationA->IsCurrentGeneration(), TEXT("A is current")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(
				&Engine,
				ECompileType::SoftReloadOnly,
				SnapshotModuleName,
				TEXT("CanonicalASTSnapshotReload.as"),
				SourceB,
				ReloadResult),
			TEXT("generation B hot reload should compile")));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			TEXT("generation B should be a handled reload")));

		const TSharedPtr<FAngelscriptModuleDesc> RecordB = Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		ASSERT_THAT(IsTrue(RecordB.IsValid() && RecordB->ScriptModule != nullptr,
			TEXT("generation B must expose ScriptModule")));
		asIASTSnapshot* GenerationB = RecordB->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationB, TEXT("generation B snapshot")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(), TEXT("old lease is not current")));
		ASSERT_THAT(IsTrue(GenerationB->IsCurrentGeneration(), TEXT("new snapshot is current")));
		asSASTDeclView View = {};
		View.structSize = sizeof(View);
		ASSERT_THAT(AreEqual(0, GenerationA->GetDecl(GenerationA->GetTranslationUnitDecl(), &View),
			TEXT("old Hot Reload lease remains traversable")));
		GenerationB->Release();
		GenerationA->Release();
	}

#if AS_CAN_GENERATE_JIT
	TEST_METHOD(TypedAstJitLeaseCompletesAgainstGenerationAAfterBPublishes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*SnapshotModuleName.ToString());
		};

		const FString SourceA = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 1;
				}
			}
			)AS");
		const FString SourceB = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 2;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SnapshotModuleName, TEXT("CanonicalASTSnapshotReload.as"), SourceA),
			TEXT("generation A should compile")));
		const TSharedPtr<FAngelscriptModuleDesc> RecordA = Engine.GetModuleByModuleName(SnapshotModuleName.ToString());
		asIASTSnapshot* GenerationA = RecordA->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		ASSERT_THAT(IsNotNull(GenerationA, TEXT("generation A lease")));
		asCASTSnapshot* const LeaseA = static_cast<asCASTSnapshot*>(GenerationA);
		ASSERT_THAT(IsNotNull(LeaseA->GetContext(), TEXT("lease exposes sealed context")));

		FAngelscriptTypedASTJITEligibilityInput InputA;
		InputA.bIsUFunctionRoot = true;
		InputA.SealedAST = LeaseA->GetContext();
		const FAngelscriptTypedASTJITEligibility EligibilityA =
			EvaluateAngelscriptTypedASTJITEligibility(InputA);
		ASSERT_THAT(AreNotEqual(
			static_cast<int32>(EAngelscriptTypedASTJITFallbackReason::MissingCanonicalAST),
			static_cast<int32>(EligibilityA.Reason),
			TEXT("leased generation A is body meaning for TypedASTJIT")));

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SnapshotModuleName, TEXT("CanonicalASTSnapshotReload.as"), SourceB),
			TEXT("generation B should compile")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(),
			TEXT("generation A is no longer current")));
		const FAngelscriptTypedASTJITEligibility EligibilityAfterB =
			EvaluateAngelscriptTypedASTJITEligibility(InputA);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(EligibilityA.Reason),
			static_cast<int32>(EligibilityAfterB.Reason),
			TEXT("in-flight analysis completes against leased generation A")));
		ASSERT_THAT(IsTrue(
			EligibilityA.Detail == EligibilityAfterB.Detail,
			TEXT("provider selection uses sealed snapshot identity, not live AST addresses")));
		GenerationA->Release();
	}
#endif

	TEST_METHOD(FailedReloadKeepsPreviousSnapshot)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*FailModuleName.ToString());
		};

		TestRunner->AddExpectedError(TEXT("CanonicalASTSnapshotReloadFail.as"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("unknown_symbol"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 0);

		const FString SourceA = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadFail : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 1;
				}
			}
			)AS");
		const FString SourceBad = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadFail : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					unknown_symbol;
					return 1;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(
				&Engine,
				FailModuleName,
				TEXT("CanonicalASTSnapshotReloadFail.as"),
				SourceA),
			TEXT("valid generation should compile")));
		const TSharedPtr<FAngelscriptModuleDesc> Record = Engine.GetModuleByModuleName(FailModuleName.ToString());
		asIASTSnapshot* GenerationA = Record.IsValid() && Record->ScriptModule != nullptr
			? Record->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1)
			: nullptr;
		ASSERT_THAT(IsNotNull(GenerationA, TEXT("retain snapshot before failed reload")));
		const FString KeyA = UTF8_TO_TCHAR(GenerationA->GetGenerationKey());

		ECompileResult ReloadResult = ECompileResult::Error;
		CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			FailModuleName,
			TEXT("CanonicalASTSnapshotReloadFail.as"),
			SourceBad,
			ReloadResult);
		const TSharedPtr<FAngelscriptModuleDesc> After = Engine.GetModuleByModuleName(FailModuleName.ToString());
		asIASTSnapshot* Kept = After.IsValid() && After->ScriptModule != nullptr
			? After->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1)
			: nullptr;
		ASSERT_THAT(IsNotNull(Kept, TEXT("failed replacement keeps the previous snapshot")));
		ASSERT_THAT(IsTrue(Kept->IsCurrentGeneration(), TEXT("kept snapshot stays current")));
		ASSERT_THAT(AreEqual(KeyA, FString(UTF8_TO_TCHAR(Kept->GetGenerationKey())),
			TEXT("failed replacement must not publish a new generation")));
		Kept->Release();
		GenerationA->Release();
	}

	TEST_METHOD(ReaderThreadSeesOnlySealedSnapshots)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*ConcurrentModuleName.ToString());
		};

		const FString SourceA = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotConcurrent : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 1;
				}
			}
			)AS");
		const FString SourceB = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotConcurrent : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 2;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(
				&Engine,
				ConcurrentModuleName,
				TEXT("CanonicalASTSnapshotConcurrent.as"),
				SourceA)));
		const TSharedPtr<FAngelscriptModuleDesc> Record = Engine.GetModuleByModuleName(ConcurrentModuleName.ToString());
		asIASTSnapshot* GenerationA = Record.IsValid() && Record->ScriptModule != nullptr
			? Record->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1)
			: nullptr;
		ASSERT_THAT(IsNotNull(GenerationA));

		FThreadSafeBool bStop = false;
		FThreadSafeBool bSawPartial = false;
		TFuture<void> Reader = Async(EAsyncExecution::Thread, [GenerationA, &bStop, &bSawPartial]()
		{
			while (!bStop)
			{
				asSASTDeclView View = {};
				View.structSize = sizeof(View);
				if (GenerationA->GetDecl(GenerationA->GetTranslationUnitDecl(), &View) != 0
					|| View.apiVersion != asAST_API_VERSION_1
					|| View.structSize == 0)
				{
					bSawPartial = true;
					break;
				}
			}
		});

		ECompileResult ReloadResult = ECompileResult::Error;
		CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			ConcurrentModuleName,
			TEXT("CanonicalASTSnapshotConcurrent.as"),
			SourceB,
			ReloadResult);
		bStop = true;
		Reader.Wait();
		ASSERT_THAT(IsFalse((bool)bSawPartial,
			TEXT("readers must not observe a partially sealed or replaced snapshot")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(),
			TEXT("generation A is no longer current after serialized publication")));
		GenerationA->Release();
	}

	TEST_METHOD(HotReloadSidecarReplacementHashesAndRejectsDumpInput)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		Engine.NextCompileASTRetentionPolicy = static_cast<int32>(asAST_RETAIN_SNAPSHOT);
		ON_SCOPE_EXIT
		{
			Engine.NextCompileASTRetentionPolicy = 0;
			Engine.DiscardModule(*SnapshotModuleName.ToString());
		};

		const FString SourceA = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 1;
				}
			}
			)AS");
		const FString SourceB = ASTEST_AS(R"AS(
			UCLASS()
			class UCanonicalASTSnapshotReloadActor : UObject
			{
				UFUNCTION()
				int GetMarker()
				{
					return 2;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SnapshotModuleName, TEXT("CanonicalASTSnapshotReload.as"), SourceA),
			TEXT("generation A should compile")));
		asIASTSnapshot* GenerationA = Engine.GetModuleByModuleName(SnapshotModuleName.ToString())->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		asCASTSnapshot* SnapA = static_cast<asCASTSnapshot*>(GenerationA);
		ASSERT_THAT(IsNotNull(SnapA, TEXT("generation A snapshot adapter")));
		ASSERT_THAT(IsNotNull(SnapA->GetContext(), TEXT("generation A sealed context")));
		asSASTSidecarHeader Header;
		Header.functionKey = "CanonicalASTSnapshotReload";
		Header.profile = "Win64";
		asCArray<asBYTE> BytesA;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(*SnapA->GetContext(), Header, BytesA),
			TEXT("generation A sidecar")));
		asCString HashA;
		asCASTContentHash(BytesA.AddressOf(), BytesA.GetLength(), HashA);

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(
				&Engine,
				ECompileType::SoftReloadOnly,
				SnapshotModuleName,
				TEXT("CanonicalASTSnapshotReload.as"),
				SourceB,
				ReloadResult),
			TEXT("generation B hot reload")));
		asIASTSnapshot* GenerationB = Engine.GetModuleByModuleName(SnapshotModuleName.ToString())->ScriptModule->AcquireASTSnapshot(asAST_API_VERSION_1);
		asCASTSnapshot* SnapB = static_cast<asCASTSnapshot*>(GenerationB);
		asCArray<asBYTE> BytesB;
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_OK, asCASTEncodeSidecar(*SnapB->GetContext(), Header, BytesB),
			TEXT("generation B sidecar")));
		asCString HashB;
		asCASTContentHash(BytesB.AddressOf(), BytesB.GetLength(), HashB);
		ASSERT_THAT(IsFalse(HashA.Equals(HashB), TEXT("sidecar content hash changes with generation")));
		ASSERT_THAT(IsFalse(GenerationA->IsCurrentGeneration(), TEXT("old snapshot lease is not current")));
		ASSERT_THAT(IsTrue(GenerationB->IsCurrentGeneration(), TEXT("new generation is current")));

		const asBYTE DumpInput[] = { 'A', 'S', 'T', '\n' };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNSUPPORTED_INPUT,
			asCASTRejectNonSidecarInput(DumpInput, 4),
			TEXT("AST dump is not a Provider or Cache input")));
		const asBYTE OldSchema[] = { 'C', 'A', 'S', 'T', 0, 0, 0, 0 };
		ASSERT_THAT(AreEqual((int)asAST_SIDECAR_UNKNOWN_KIND,
			asCASTRejectNonSidecarInput(OldSchema, 8),
			TEXT("old schema is a safe miss")));
		GenerationB->Release();
		GenerationA->Release();
	}
};

#endif
