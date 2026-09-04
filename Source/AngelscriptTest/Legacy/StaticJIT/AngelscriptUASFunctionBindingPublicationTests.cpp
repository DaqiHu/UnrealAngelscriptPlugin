#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Async/Async.h"
#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

#include <atomic>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptUASFunctionBindingPublicationTests_Private
{
	struct FIntReturnParams
	{
		int32 ReturnValue = 0;
	};

	static int32 OldBindingTag = 0;
	static int32 NewBindingTag = 0;
	static FEvent* BlockingEntryStarted = nullptr;
	static FEvent* BlockingEntryContinue = nullptr;
	static std::atomic<int32> OldBlockingCalls{0};
	static std::atomic<int32> OldReentrantCalls{0};
	static std::atomic<int32> NewCalls{0};
	static std::atomic<int32> OldReleaseCalls{0};
	static std::atomic<int32> NewReleaseCalls{0};
	static std::atomic<int32> ReentrantPublishResult{asERROR};
	static std::atomic<int32> ReentrantNestedPrepareResult{asERROR};
	static std::atomic<int32> ReentrantNestedSetObjectResult{asERROR};
	static std::atomic<int32> ReentrantNestedExecuteResult{asERROR};
	static std::atomic<asDWORD> ReentrantNestedValue{0};
	static std::atomic<int32> OldReleasesObservedInsideEntry{-1};

	static void ResetState()
	{
		OldBlockingCalls.store(0);
		OldReentrantCalls.store(0);
		NewCalls.store(0);
		OldReleaseCalls.store(0);
		NewReleaseCalls.store(0);
		ReentrantPublishResult.store(asERROR);
		ReentrantNestedPrepareResult.store(asERROR);
		ReentrantNestedSetObjectResult.store(asERROR);
		ReentrantNestedExecuteResult.store(asERROR);
		ReentrantNestedValue.store(0);
		OldReleasesObservedInsideEntry.store(-1);
	}

	static asDWORD OldRawEntry(FScriptExecution&, void*)
	{
		return 801;
	}

	static asDWORD NewRawEntry(FScriptExecution&, void*)
	{
		return 902;
	}

	static void OldParmsEntry(FScriptExecution&, void*, void* Parms)
	{
		static_cast<FIntReturnParams*>(Parms)->ReturnValue = 801;
	}

	static void NewParmsEntry(FScriptExecution&, void*, void* Parms)
	{
		static_cast<FIntReturnParams*>(Parms)->ReturnValue = 902;
	}

	static asSJITFunctionBinding MakeNewBinding();

	static void OldBlockingVMEntry(
		FScriptExecution& Execution,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (Execution.jitBindingUserData == &OldBindingTag)
		{
			OldBlockingCalls.fetch_add(1);
		}
		if (BlockingEntryStarted != nullptr)
		{
			BlockingEntryStarted->Trigger();
		}
		if (BlockingEntryContinue != nullptr)
		{
			BlockingEntryContinue->Wait(5000);
		}
		if (OutValue != nullptr)
		{
			*OutValue = 801;
		}
	}

	static void NewVMEntry(
		FScriptExecution& Execution,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (Execution.jitBindingUserData == &NewBindingTag)
		{
			NewCalls.fetch_add(1);
		}
		if (OutValue != nullptr)
		{
			*OutValue = 902;
		}
	}

	static void OldReentrantVMEntry(
		FScriptExecution& Execution,
		asDWORD* FramePointer,
		asQWORD* OutValue)
	{
		if (Execution.jitBindingUserData == &OldBindingTag)
		{
			OldReentrantCalls.fetch_add(1);
		}

		ReentrantPublishResult.store(
			Execution.jitFunction->SetJITBinding(MakeNewBinding()));

		asIScriptContext* const NestedContext =
			Execution.jitEngine->CreateContext();
		if (NestedContext == nullptr)
		{
			OldReleasesObservedInsideEntry.store(OldReleaseCalls.load());
			return;
		}

		const int32 PrepareResult = NestedContext->Prepare(
			Execution.jitFunction);
		ReentrantNestedPrepareResult.store(PrepareResult);
		if (PrepareResult >= 0)
		{
			void* const Object = FramePointer != nullptr
				? *reinterpret_cast<void**>(FramePointer)
				: nullptr;
			const int32 SetObjectResult = NestedContext->SetObject(Object);
			ReentrantNestedSetObjectResult.store(SetObjectResult);
			if (SetObjectResult >= 0)
			{
				const int32 ExecuteResult = NestedContext->Execute();
				ReentrantNestedExecuteResult.store(ExecuteResult);
				if (ExecuteResult == asEXECUTION_FINISHED)
				{
					ReentrantNestedValue.store(
						NestedContext->GetReturnDWord());
				}
			}
		}
		NestedContext->Release();

		OldReleasesObservedInsideEntry.store(OldReleaseCalls.load());
		if (OutValue != nullptr)
		{
			*OutValue = ReentrantNestedValue.load() + 1;
		}
	}

	static asSJITFunctionBinding MakeOldBlockingBinding()
	{
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &OldBlockingVMEntry;
		Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(&OldRawEntry);
		Binding.ParmsEntry = &OldParmsEntry;
		Binding.UserData = &OldBindingTag;
		return Binding;
	}

	static asSJITFunctionBinding MakeOldReentrantBinding()
	{
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &OldReentrantVMEntry;
		Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(&OldRawEntry);
		Binding.ParmsEntry = &OldParmsEntry;
		Binding.UserData = &OldBindingTag;
		return Binding;
	}

	static asSJITFunctionBinding MakeNewBinding()
	{
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &NewVMEntry;
		Binding.RawEntry = reinterpret_cast<asJITFunction_Raw>(&NewRawEntry);
		Binding.ParmsEntry = &NewParmsEntry;
		Binding.UserData = &NewBindingTag;
		return Binding;
	}

	class FPublicationCompiler final : public asIJITCompiler
	{
	public:
		void OnFunctionReady(asIScriptFunction* Function) override
		{
			if (Function == nullptr)
			{
				return;
			}

			if (FCStringAnsi::Strcmp(
					Function->GetName(),
					"ThreadSafeValue") == 0)
			{
				Function->SetJITBinding(MakeOldBlockingBinding());
			}
			else if (FCStringAnsi::Strcmp(
					Function->GetName(),
					"ReentrantValue") == 0)
			{
				Function->SetJITBinding(MakeOldReentrantBinding());
			}
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding& Binding) override
		{
			if (Binding.UserData == &OldBindingTag)
			{
				OldReleaseCalls.fetch_add(1);
			}
			else if (Binding.UserData == &NewBindingTag)
			{
				NewReleaseCalls.fetch_add(1);
			}
		}
	};

	static UASFunction* CompileThreadSafeFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ClassName,
		const FString& FunctionName)
	{
		FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class __CLASS_NAME__ : UObject
			{
				UFUNCTION(meta = (BlueprintThreadSafe))
				int __FUNCTION_NAME__() final
				{
					return 1;
				}
			}
			)AS");
		Source.ReplaceInline(TEXT("__CLASS_NAME__"), *ClassName);
		Source.ReplaceInline(TEXT("__FUNCTION_NAME__"), *FunctionName);

		UClass* const GeneratedClass =
			AngelscriptFunctionalTestUtils::CompileScriptModule(
				Test,
				Engine,
				ModuleName,
				Filename,
				Source,
				FName(*ClassName));
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		return Cast<UASFunction>(FindGeneratedFunction(
			GeneratedClass,
			FName(*FunctionName)));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptUASFunctionBindingPublicationTests,
	"Angelscript.TestModule.StaticJIT.UASFunction.BindingPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ThreadSafeCallRetainsRetiredBindingUntilItReturns)
	{
		using namespace AngelscriptUASFunctionBindingPublicationTests_Private;

		ResetState();
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asPWORD OriginalIncludeJIT = ScriptEngine->GetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS);
		asIJITCompiler* const OriginalCompiler = ScriptEngine->GetJITCompiler();
		FPublicationCompiler Compiler;
		const FName ModuleName(TEXT("UASFunctionThreadSafePublication"));
		ON_SCOPE_EXIT
		{
			if (BlockingEntryContinue != nullptr)
			{
				BlockingEntryContinue->Trigger();
			}
			Engine.DiscardModule(*ModuleName.ToString());
			ScriptEngine->SetJITCompiler(OriginalCompiler);
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				OriginalIncludeJIT);
			if (BlockingEntryStarted != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(BlockingEntryStarted);
				BlockingEntryStarted = nullptr;
			}
			if (BlockingEntryContinue != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(BlockingEntryContinue);
				BlockingEntryContinue = nullptr;
			}
		};

		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				1)));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));

		UASFunction* const Wrapper = CompileThreadSafeFunction(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("UASFunctionThreadSafePublication.as"),
			TEXT("UUASFunctionThreadSafePublicationTarget"),
			TEXT("ThreadSafeValue"));
		ASSERT_THAT(IsNotNull(Wrapper));
		ASSERT_THAT(IsTrue(Wrapper != nullptr
			&& Wrapper->GetClass() == UASFunction::StaticClass()));
		if (Wrapper == nullptr || Wrapper->ScriptFunction == nullptr)
		{
			return;
		}

		UObject* const Instance = NewObject<UObject>(
			GetTransientPackage(),
			Wrapper->GetOwnerClass(),
			TEXT("UASFunctionThreadSafePublicationInstance"));
		ASSERT_THAT(IsNotNull(Instance));
		if (Instance == nullptr)
		{
			return;
		}

		asIScriptFunction* const ScriptFunction = Wrapper->ScriptFunction;
		const asSJITFunctionBinding InitialBinding =
			ScriptFunction->GetJITBinding();
		ASSERT_THAT(IsTrue(InitialBinding.VMEntry == &OldBlockingVMEntry));
		ASSERT_THAT(IsTrue(InitialBinding.RawEntry
			== reinterpret_cast<asJITFunction_Raw>(&OldRawEntry)));
		ASSERT_THAT(IsTrue(InitialBinding.ParmsEntry == &OldParmsEntry));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&OldBindingTag),
			InitialBinding.UserData));

		BlockingEntryStarted = FPlatformProcess::GetSynchEventFromPool(true);
		BlockingEntryContinue = FPlatformProcess::GetSynchEventFromPool(true);
		TFuture<FIntReturnParams> OldCall = Async(
			EAsyncExecution::Thread,
			[Wrapper, Instance]()
			{
				FIntReturnParams Params;
				Wrapper->RuntimeCallEvent(Instance, &Params);
				return Params;
			});

		const bool bOldEntryStarted = BlockingEntryStarted->Wait(5000);
		ASSERT_THAT(IsTrue(bOldEntryStarted));
		if (!bOldEntryStarted)
		{
			BlockingEntryContinue->Trigger();
			OldCall.Wait();
			return;
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptFunction->SetJITBinding(MakeNewBinding())));
		ASSERT_THAT(AreEqual(0, OldReleaseCalls.load()));
		ASSERT_THAT(IsTrue(
			ScriptFunction->GetJITBinding().VMEntry == &NewVMEntry));

		FIntReturnParams NewParams;
		Wrapper->RuntimeCallEvent(Instance, &NewParams);
		ASSERT_THAT(AreEqual(902, NewParams.ReturnValue));
		ASSERT_THAT(AreEqual(1, NewCalls.load()));
		ASSERT_THAT(AreEqual(
			0,
			OldReleaseCalls.load(),
			TEXT("New-generation calls must not retire an active old snapshot")));

		BlockingEntryContinue->Trigger();
		const FIntReturnParams OldParams = OldCall.Get();
		ASSERT_THAT(AreEqual(801, OldParams.ReturnValue));
		ASSERT_THAT(AreEqual(1, OldBlockingCalls.load()));
		ASSERT_THAT(AreEqual(
			1,
			OldReleaseCalls.load(),
			TEXT("Old snapshot should retire exactly once after its last reader exits")));
		ASSERT_THAT(AreEqual(0, NewReleaseCalls.load()));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptFunction->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(AreEqual(1, OldReleaseCalls.load()));
		ASSERT_THAT(AreEqual(1, NewReleaseCalls.load()));
	}

	TEST_METHOD(ReentrantCallPublishesAndObservesNewBindingSafely)
	{
		using namespace AngelscriptUASFunctionBindingPublicationTests_Private;

		ResetState();
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const asPWORD OriginalIncludeJIT = ScriptEngine->GetEngineProperty(
			asEP_INCLUDE_JIT_INSTRUCTIONS);
		asIJITCompiler* const OriginalCompiler = ScriptEngine->GetJITCompiler();
		FPublicationCompiler Compiler;
		const FName ModuleName(TEXT("UASFunctionReentrantPublication"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ScriptEngine->SetJITCompiler(OriginalCompiler);
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				OriginalIncludeJIT);
		};

		ASSERT_THAT(AreEqual(asSUCCESS,
			ScriptEngine->SetEngineProperty(
				asEP_INCLUDE_JIT_INSTRUCTIONS,
				1)));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler)));

		UASFunction* const Wrapper = CompileThreadSafeFunction(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("UASFunctionReentrantPublication.as"),
			TEXT("UUASFunctionReentrantPublicationTarget"),
			TEXT("ReentrantValue"));
		ASSERT_THAT(IsNotNull(Wrapper));
		ASSERT_THAT(IsTrue(Wrapper != nullptr
			&& Wrapper->GetClass() == UASFunction::StaticClass()));
		if (Wrapper == nullptr || Wrapper->ScriptFunction == nullptr)
		{
			return;
		}

		UObject* const Instance = NewObject<UObject>(
			GetTransientPackage(),
			Wrapper->GetOwnerClass(),
			TEXT("UASFunctionReentrantPublicationInstance"));
		ASSERT_THAT(IsNotNull(Instance));
		if (Instance == nullptr)
		{
			return;
		}

		asIScriptFunction* const ScriptFunction = Wrapper->ScriptFunction;
		ASSERT_THAT(IsTrue(
			ScriptFunction->GetJITBinding().VMEntry
				== &OldReentrantVMEntry));

		FIntReturnParams OuterParams;
		Wrapper->RuntimeCallEvent(Instance, &OuterParams);
		ASSERT_THAT(AreEqual(903, OuterParams.ReturnValue));
		ASSERT_THAT(AreEqual(1, OldReentrantCalls.load()));
		ASSERT_THAT(AreEqual(asSUCCESS, ReentrantPublishResult.load()));
		ASSERT_THAT(AreEqual(asSUCCESS,
			ReentrantNestedPrepareResult.load()));
		ASSERT_THAT(IsTrue(ReentrantNestedSetObjectResult.load() >= 0));
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED,
			ReentrantNestedExecuteResult.load()));
		ASSERT_THAT(AreEqual(902u, ReentrantNestedValue.load()));
		ASSERT_THAT(AreEqual(
			0,
			OldReleasesObservedInsideEntry.load(),
			TEXT("Re-entrant replacement must retain the outer binding")));
		ASSERT_THAT(AreEqual(1, NewCalls.load()));
		ASSERT_THAT(AreEqual(1, OldReleaseCalls.load()));
		ASSERT_THAT(AreEqual(0, NewReleaseCalls.load()));
		ASSERT_THAT(IsTrue(
			ScriptFunction->GetJITBinding().VMEntry == &NewVMEntry));

		FIntReturnParams LaterParams;
		Wrapper->RuntimeCallEvent(Instance, &LaterParams);
		ASSERT_THAT(AreEqual(902, LaterParams.ReturnValue));
		ASSERT_THAT(AreEqual(2, NewCalls.load()));
		ASSERT_THAT(AreEqual(1, OldReleaseCalls.load()));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptFunction->SetJITBinding(asSJITFunctionBinding())));
		ASSERT_THAT(AreEqual(1, OldReleaseCalls.load()));
		ASSERT_THAT(AreEqual(1, NewReleaseCalls.load()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
