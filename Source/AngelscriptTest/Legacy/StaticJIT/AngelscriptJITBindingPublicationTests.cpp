#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "RuntimeJIT/AngelscriptJITCoordinator.h"

#include "AngelscriptTestMacros.h"

#include "Async/Async.h"
#include "CQTest.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITBindingPublicationTests_Private
{
	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-binding-publication-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static asIScriptEngine* ExpectedEngine = nullptr;
	static asIScriptFunction* ExpectedFunction = nullptr;
	static const FAngelscriptJITResolvedReferenceTable* ExpectedTable = nullptr;
	static bool bObservedCompleteContext = false;
	static FEvent* BlockingEntryStarted = nullptr;
	static FEvent* BlockingEntryContinue = nullptr;

	static void ProviderVmEntry(
		FScriptExecution& Execution,
		asDWORD*,
		asQWORD* OutValue)
	{
		bObservedCompleteContext = Execution.jitEngine == ExpectedEngine
			&& Execution.jitFunction == ExpectedFunction
			&& Execution.jitBinding != nullptr
			&& Execution.jitBinding->UserData != nullptr
			&& Execution.jitBindingUserData == Execution.jitBinding->UserData
			&& FAngelscriptJITExecutionContext::GetReferenceTable(Execution)
				== ExpectedTable;
		if (OutValue != nullptr)
		{
			*OutValue = 88;
		}
	}

	static void ProviderRawEntry()
	{
	}

	static void BlockingProviderVmEntry(
		FScriptExecution& Execution,
		asDWORD*,
		asQWORD* OutValue)
	{
		bObservedCompleteContext =
			FAngelscriptJITExecutionContext::GetReferenceTable(Execution)
				== ExpectedTable;
		if (BlockingEntryStarted != nullptr)
		{
			BlockingEntryStarted->Trigger();
		}
		if (BlockingEntryContinue != nullptr)
		{
			BlockingEntryContinue->Wait(5000);
		}
		bObservedCompleteContext = bObservedCompleteContext
			&& FAngelscriptJITExecutionContext::GetReferenceTable(Execution)
				== ExpectedTable;
		if (OutValue != nullptr)
		{
			*OutValue = 89;
		}
	}

	class FProvider final : public IAngelscriptJITArtifactProvider
	{
	public:
		FString ProviderName;
		FString OwnerModuleName = TEXT("AngelscriptTest");
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProvider(const TCHAR* Seed, asJITFunction EntryPoint)
			: ProviderName(FString::Printf(TEXT("Provider-%s"), Seed))
		{
			View.ProviderId.Hash = MakeHash(Seed);
			View.ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			View.NativeEnvironmentFingerprint = MakeHash(TEXT("environment"));
			Entries.SetNum(1);
			Entries[0].ModuleKey.Hash = MakeHash(FString::Printf(
				TEXT("module-%s"), Seed));
			Entries[0].FunctionKey.Hash = MakeHash(FString::Printf(
				TEXT("function-%s"), Seed));
			Entries[0].ExecutionHash = MakeHash(FString::Printf(
				TEXT("execution-%s"), Seed));
			Entries[0].DebugHash = MakeHash(FString::Printf(
				TEXT("debug-%s"), Seed));
			Entries[0].ArtifactProfile = View.ArtifactProfile;
			Entries[0].EntryAbiHash = MakeHash(FString::Printf(
				TEXT("abi-%s"), Seed));
			Entries[0].NativeEnvironmentFingerprint =
				View.NativeEnvironmentFingerprint;
			Entries[0].EntryPoints.VMEntry = EntryPoint;
			View.ProviderName = *ProviderName;
			View.OwnerModuleName = *OwnerModuleName;
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(Entries);
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITBindingPublicationTests,
	"Angelscript.TestModule.StaticJIT.BindingPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PublishedBindingRetainsProviderAndReferenceSnapshotsUntilRetirement)
	{
		using namespace AngelscriptJITBindingPublicationTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		FAngelscriptJITCoordinator Compiler(ScriptEngine);
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner, Engine, "JitBindingPublication", Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(), "int Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			ScriptEngine->SetJITCompiler(nullptr);
			return;
		}

		TSharedPtr<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>();
		Catalog->Entries.SetNum(1);
		Catalog->Entries[0].EntryPoints.VMEntry = &ProviderVmEntry;
		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> Table =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		int32 ReferenceValue = 91;
		FAngelscriptJITResolvedReferenceSlot& Slot =
			Table->Slots.AddDefaulted_GetRef();
		Slot.Value = &ReferenceValue;

		TWeakPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> WeakCatalog = Catalog;
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakTable = Table;
		ExpectedEngine = ScriptEngine;
		ExpectedFunction = Function;
		ExpectedTable = Table.Get();
		FAngelscriptJITArtifactSelection Selection;
		Selection.Provider = Catalog;
		Selection.EntryIndex = 0;
		Selection.ReferenceTable = Table;
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function, Selection)));
		Selection = {};
		Catalog.Reset();
		Table.Reset();
		ASSERT_THAT(IsTrue(WeakCatalog.IsValid()));
		ASSERT_THAT(IsTrue(WeakTable.IsValid()));

		bObservedCompleteContext = false;
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(88u, Context->GetReturnDWord()));
			Context->Release();
		}
		ASSERT_THAT(IsTrue(bObservedCompleteContext));

		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetJITCompiler(nullptr)));
		ASSERT_THAT(IsFalse(WeakCatalog.IsValid()));
		ASSERT_THAT(IsFalse(WeakTable.IsValid()));
		const asSJITFunctionBinding Retired = Function->GetJITBinding();
		ASSERT_THAT(IsNull(Retired.VMEntry));
		ASSERT_THAT(IsNull(Retired.UserData));
	}

	TEST_METHOD(OverlappingNativeReaderRetainsOldGenerationUntilEntryReturns)
	{
		using namespace AngelscriptJITBindingPublicationTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		FAngelscriptJITCoordinator Compiler(ScriptEngine);
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner, Engine, "JitBindingReaderLease", Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(), "int Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		auto MakeSelection = [](asJITFunction Entry)
		{
			TSharedPtr<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe> Catalog =
				MakeShared<FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>();
			Catalog->Entries.SetNum(1);
			Catalog->Entries[0].EntryPoints.VMEntry = Entry;
			Catalog->Lifetime = FAngelscriptJITProviderLifetime::CreateLogical(
				FAngelscriptJITProviderId{MakeHash(TEXT("binding-provider"))},
				FAngelscriptJITProviderGeneration{MakeHash(
					Entry == &BlockingProviderVmEntry
						? TEXT("binding-old-generation")
						: TEXT("binding-new-generation"))},
				TEXT("AngelscriptTest"));
			TSharedPtr<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe> Table =
				MakeShared<FAngelscriptJITResolvedReferenceTable,
					ESPMode::ThreadSafe>();
			FAngelscriptJITArtifactSelection Selection;
			Selection.Provider = Catalog;
			Selection.EntryIndex = 0;
			Selection.ReferenceTable = Table;
			return Selection;
		};
		FAngelscriptJITArtifactSelection OldSelection =
			MakeSelection(&BlockingProviderVmEntry);
		FAngelscriptJITArtifactSelection NewSelection =
			MakeSelection(&ProviderVmEntry);
		TWeakPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> WeakOldCatalog = OldSelection.Provider;
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakOldTable = OldSelection.ReferenceTable;
		TWeakPtr<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe> WeakOldLifetime =
			OldSelection.Provider->Lifetime;
		ExpectedEngine = ScriptEngine;
		ExpectedFunction = Function;
		ExpectedTable = OldSelection.ReferenceTable.Get();
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function, OldSelection)));
		OldSelection = {};

		BlockingEntryStarted = FPlatformProcess::GetSynchEventFromPool(true);
		BlockingEntryContinue = FPlatformProcess::GetSynchEventFromPool(true);
		ON_SCOPE_EXIT
		{
			if (BlockingEntryContinue != nullptr)
			{
				BlockingEntryContinue->Trigger();
			}
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryStarted);
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryContinue);
			BlockingEntryStarted = nullptr;
			BlockingEntryContinue = nullptr;
		};

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		TFuture<TPair<int32, asDWORD>> Execution = Async(
			EAsyncExecution::Thread,
			[Context]()
			{
				const int32 Result = Context->Execute();
				const asDWORD Value = Context->GetReturnDWord();
				Context->Release();
				return TPair<int32, asDWORD>(Result, Value);
			});
		const bool bEntryStarted = BlockingEntryStarted->Wait(5000);
		ASSERT_THAT(IsTrue(bEntryStarted));
		if (!bEntryStarted)
		{
			BlockingEntryContinue->Trigger();
			Execution.Wait();
			return;
		}

		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function, NewSelection)));
		ASSERT_THAT(IsTrue(WeakOldCatalog.IsValid()));
		ASSERT_THAT(IsTrue(WeakOldTable.IsValid()));
		ASSERT_THAT(IsTrue(WeakOldLifetime.IsValid()));
		BlockingEntryContinue->Trigger();
		const TPair<int32, asDWORD> ExecutionResult = Execution.Get();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecutionResult.Key));
		ASSERT_THAT(AreEqual(89u, ExecutionResult.Value));
		ASSERT_THAT(IsTrue(bObservedCompleteContext));
		ASSERT_THAT(IsFalse(WeakOldCatalog.IsValid()));
		ASSERT_THAT(IsFalse(WeakOldTable.IsValid()));
		ASSERT_THAT(IsFalse(WeakOldLifetime.IsValid()));
		ASSERT_THAT(IsTrue(Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));
	}

	TEST_METHOD(ExternalCallerLeaseRetainsCompleteBindingUntilInvocationReturns)
	{
		using namespace AngelscriptJITBindingPublicationTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		FAngelscriptJITCoordinator Compiler(ScriptEngine);
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			void Entry() {}
			)AS");
		FScopedNativeModule Module(
			*TestRunner, Engine, "JitExternalBindingLease", Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(), "void Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		TSharedPtr<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>();
		Catalog->Entries.SetNum(1);
		Catalog->Entries[0].EntryPoints.RawEntry = &ProviderRawEntry;
		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> Table =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		FAngelscriptJITArtifactSelection Selection;
		Selection.Provider = Catalog;
		Selection.EntryIndex = 0;
		Selection.ReferenceTable = Table;
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function, Selection)));
		TWeakPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> WeakCatalog = Catalog;
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakTable = Table;
		Catalog.Reset();
		Table.Reset();
		Selection = {};

		bool bObservedExternalContext = false;
		{
			FAngelscriptJITBindingExecutionLease Lease(Function);
			ASSERT_THAT(IsTrue(Lease.GetBinding().RawEntry
				== &ProviderRawEntry));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Function->SetJITBinding(asSJITFunctionBinding())));
			ASSERT_THAT(IsTrue(WeakCatalog.IsValid()));
			ASSERT_THAT(IsTrue(WeakTable.IsValid()));
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			ASSERT_THAT(IsTrue(FAngelscriptJITExecutionContext::InvokeRaw(
				Execution,
				ScriptEngine,
				Function,
				Lease.GetBinding(),
				[&bObservedExternalContext](
					asJITFunction_Raw,
					FScriptExecution& ScopedExecution)
				{
					bObservedExternalContext =
						FAngelscriptJITExecutionContext::GetReferenceTable(
							ScopedExecution) != nullptr;
				})));
		}

		ASSERT_THAT(IsTrue(bObservedExternalContext));
		ASSERT_THAT(IsFalse(WeakCatalog.IsValid()));
		ASSERT_THAT(IsFalse(WeakTable.IsValid()));
	}

	TEST_METHOD(UnregisterDuringActiveCallStopsNewSelectionAndRetainsOldCodeLease)
	{
		using namespace AngelscriptJITBindingPublicationTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		FAngelscriptJITCoordinator Compiler(ScriptEngine);
		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 1;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner, Engine, "JitProviderUnregisterActiveCall", Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(), "int Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FAngelscriptJITProviderRegistry Registry;
		FProvider RetiringProvider(TEXT("retiring"), &BlockingProviderVmEntry);
		FProvider UnrelatedProvider(TEXT("unrelated"), &ProviderVmEntry);
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(
			&RetiringProvider, RetiringProvider).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(
			&UnrelatedProvider, UnrelatedProvider).IsAccepted()));

		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> RetiringCatalog;
		for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Catalog : Snapshot->Providers)
		{
			if (Catalog->ProviderId == RetiringProvider.View.ProviderId)
			{
				RetiringCatalog = Catalog;
				break;
			}
		}
		ASSERT_THAT(IsTrue(RetiringCatalog.IsValid()));
		if (!RetiringCatalog.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(RetiringCatalog->Lifetime->KeepsCodeMapped()));

		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> Table =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		FAngelscriptJITArtifactSelection Selection;
		Selection.Provider = RetiringCatalog;
		Selection.EntryIndex = 0;
		Selection.ReferenceTable = Table;
		TWeakPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> WeakCatalog = RetiringCatalog;
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakTable = Table;
		TWeakPtr<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe> WeakLifetime = RetiringCatalog->Lifetime;
		ExpectedEngine = ScriptEngine;
		ExpectedFunction = Function;
		ExpectedTable = Table.Get();
		ASSERT_THAT(IsTrue(Compiler.PublishProviderSelection(
			Function, Selection)));
		Selection = {};
		Table.Reset();
		RetiringCatalog.Reset();
		Snapshot.Reset();

		BlockingEntryStarted = FPlatformProcess::GetSynchEventFromPool(true);
		BlockingEntryContinue = FPlatformProcess::GetSynchEventFromPool(true);
		ON_SCOPE_EXIT
		{
			if (BlockingEntryContinue != nullptr)
			{
				BlockingEntryContinue->Trigger();
			}
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryStarted);
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryContinue);
			BlockingEntryStarted = nullptr;
			BlockingEntryContinue = nullptr;
		};

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		TFuture<TPair<int32, asDWORD>> Execution = Async(
			EAsyncExecution::Thread,
			[Context]()
			{
				const int32 Result = Context->Execute();
				const asDWORD Value = Context->GetReturnDWord();
				Context->Release();
				return TPair<int32, asDWORD>(Result, Value);
			});
		const bool bEntryStarted = BlockingEntryStarted->Wait(5000);
		ASSERT_THAT(IsTrue(bEntryStarted));
		if (!bEntryStarted)
		{
			BlockingEntryContinue->Trigger();
			Execution.Wait();
			return;
		}

		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&RetiringProvider)));
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AfterUnregister = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(1, AfterUnregister->Providers.Num()));
		ASSERT_THAT(IsTrue(AfterUnregister->Providers[0]->ProviderId
			== UnrelatedProvider.View.ProviderId));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Function->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(IsTrue(WeakCatalog.IsValid()));
		ASSERT_THAT(IsTrue(WeakTable.IsValid()));
		ASSERT_THAT(IsTrue(WeakLifetime.IsValid()));

		BlockingEntryContinue->Trigger();
		const TPair<int32, asDWORD> ExecutionResult = Execution.Get();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecutionResult.Key));
		ASSERT_THAT(AreEqual(89u, ExecutionResult.Value));
		ASSERT_THAT(IsTrue(bObservedCompleteContext));
		ASSERT_THAT(IsFalse(WeakCatalog.IsValid()));
		ASSERT_THAT(IsFalse(WeakTable.IsValid()));
		ASSERT_THAT(IsFalse(WeakLifetime.IsValid()));
	}

	TEST_METHOD(TwoEnginesRetireIndependentBindingsBeforeSharedGenerationLease)
	{
		using namespace AngelscriptJITBindingPublicationTests_Private;
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine FirstEngine;
		FNativeTestEngine SecondEngine;
		FirstEngine.Create(*TestRunner);
		SecondEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			SecondEngine.Destroy();
			FirstEngine.Destroy();
		};
		asIScriptEngine* const FirstScriptEngine = FirstEngine.Get();
		asIScriptEngine* const SecondScriptEngine = SecondEngine.Get();
		ASSERT_THAT(IsNotNull(FirstScriptEngine));
		ASSERT_THAT(IsNotNull(SecondScriptEngine));
		if (FirstScriptEngine == nullptr || SecondScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, FirstScriptEngine->SetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		ASSERT_THAT(AreEqual(asSUCCESS, SecondScriptEngine->SetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS, 1)));
		FAngelscriptJITCoordinator FirstCompiler(FirstScriptEngine);
		FAngelscriptJITCoordinator SecondCompiler(SecondScriptEngine);
		ASSERT_THAT(AreEqual(asSUCCESS,
			FirstScriptEngine->SetJITCompiler(&FirstCompiler)));
		ASSERT_THAT(AreEqual(asSUCCESS,
			SecondScriptEngine->SetJITCompiler(&SecondCompiler)));
		ON_SCOPE_EXIT
		{
			SecondScriptEngine->SetJITCompiler(nullptr);
			FirstScriptEngine->SetJITCompiler(nullptr);
		};

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			void Entry() {}
			)AS");
		FScopedNativeModule FirstModule(
			*TestRunner, FirstEngine, "JitTwoEngineFirst", Source);
		FScopedNativeModule SecondModule(
			*TestRunner, SecondEngine, "JitTwoEngineSecond", Source);
		ASSERT_THAT(IsTrue(FirstModule.IsValid()));
		ASSERT_THAT(IsTrue(SecondModule.IsValid()));
		asIScriptFunction* const FirstFunction = GetNativeFunctionByDecl(
			FirstModule.Get(), "void Entry()");
		asIScriptFunction* const SecondFunction = GetNativeFunctionByDecl(
			SecondModule.Get(), "void Entry()");
		ASSERT_THAT(IsNotNull(FirstFunction));
		ASSERT_THAT(IsNotNull(SecondFunction));
		if (FirstFunction == nullptr || SecondFunction == nullptr)
		{
			return;
		}

		TSharedPtr<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>();
		Catalog->ProviderId.Hash = MakeHash(TEXT("two-engine-provider"));
		Catalog->ProviderGeneration.Hash =
			MakeHash(TEXT("two-engine-generation"));
		Catalog->Lifetime = FAngelscriptJITProviderLifetime::CreateLogical(
			Catalog->ProviderId,
			Catalog->ProviderGeneration,
			TEXT("AngelscriptTest"));
		Catalog->Entries.SetNum(1);
		Catalog->Entries[0].EntryPoints.RawEntry = &ProviderRawEntry;
		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> FirstTable =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		TSharedPtr<FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> SecondTable =
			MakeShared<FAngelscriptJITResolvedReferenceTable,
				ESPMode::ThreadSafe>();
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakFirstTable = FirstTable;
		TWeakPtr<const FAngelscriptJITResolvedReferenceTable,
			ESPMode::ThreadSafe> WeakSecondTable = SecondTable;
		TWeakPtr<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe> WeakLifetime = Catalog->Lifetime;

		FAngelscriptJITArtifactSelection FirstSelection;
		FirstSelection.Provider = Catalog;
		FirstSelection.EntryIndex = 0;
		FirstSelection.ReferenceTable = FirstTable;
		FAngelscriptJITArtifactSelection SecondSelection;
		SecondSelection.Provider = Catalog;
		SecondSelection.EntryIndex = 0;
		SecondSelection.ReferenceTable = SecondTable;
		ASSERT_THAT(IsTrue(FirstCompiler.PublishProviderSelection(
			FirstFunction, FirstSelection)));
		ASSERT_THAT(IsTrue(SecondCompiler.PublishProviderSelection(
			SecondFunction, SecondSelection)));
		FirstSelection = {};
		SecondSelection = {};
		Catalog.Reset();
		FirstTable.Reset();
		SecondTable.Reset();

		ASSERT_THAT(AreEqual(asSUCCESS,
			FirstFunction->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(IsFalse(WeakFirstTable.IsValid()));
		ASSERT_THAT(IsTrue(WeakSecondTable.IsValid()));
		ASSERT_THAT(IsTrue(WeakLifetime.IsValid()));
		ASSERT_THAT(IsTrue(SecondFunction->GetJITBinding().RawEntry
			== &ProviderRawEntry));

		ASSERT_THAT(AreEqual(asSUCCESS,
			SecondFunction->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(IsFalse(WeakSecondTable.IsValid()));
		ASSERT_THAT(IsFalse(WeakLifetime.IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
