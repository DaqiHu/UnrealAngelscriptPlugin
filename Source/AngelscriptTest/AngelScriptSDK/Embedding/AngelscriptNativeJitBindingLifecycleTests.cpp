#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeJitBindingLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.JitBindingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FRecordingJitCompiler final : public asIJITCompiler
	{
	public:
		struct FReleaseObservation
		{
			asIScriptFunction* Function = nullptr;
			asSJITFunctionBinding Binding;
			bool bObservedEmptyBinding = false;
			int32 ReentrantReturnValue = INDEX_NONE;
			int32 ReentrantExecuteResult = asERROR;
		};

		TArray<asIScriptFunction*> ReadyFunctions;
		TArray<FReleaseObservation> ReleaseObservations;
		bool bReenterRelease = false;

		void OnFunctionReady(asIScriptFunction* Function) override
		{
			ReadyFunctions.Add(Function);
		}

		void ReleaseFunctionBinding(
			asIScriptFunction* Function,
			const asSJITFunctionBinding& Binding) override
		{
			FReleaseObservation& Observation = ReleaseObservations.AddDefaulted_GetRef();
			Observation.Function = Function;
			Observation.Binding = Binding;

			if (Function == nullptr)
			{
				return;
			}

			const asSJITFunctionBinding ActiveBinding = Function->GetJITBinding();
			Observation.bObservedEmptyBinding = ActiveBinding.VMEntry == nullptr
				&& ActiveBinding.RawEntry == nullptr
				&& ActiveBinding.ParmsEntry == nullptr
				&& ActiveBinding.UserData == nullptr;

			if (!bReenterRelease)
			{
				return;
			}

			asSJITFunctionBinding EmptyBinding;
			Function->SetJITBinding(EmptyBinding);
			asIScriptContext* const Context = Function->GetEngine()->CreateContext();
			if (Context == nullptr)
			{
				return;
			}

			const int PrepareResult = Context->Prepare(Function);
			Observation.ReentrantExecuteResult = PrepareResult == asSUCCESS
				? Context->Execute()
				: PrepareResult;
			if (Observation.ReentrantExecuteResult == asEXECUTION_FINISHED)
			{
				Observation.ReentrantReturnValue = static_cast<int32>(Context->GetReturnDWord());
			}
			Context->Release();
		}
	};

	static void SentinelVmEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD*)
	{
	}

	static void SentinelRawEntry()
	{
	}

	static void SentinelParmsEntry(
		FScriptExecution&,
		void*,
		void*)
	{
	}

	static asSJITFunctionBinding MakeSentinelBinding(void* UserData)
	{
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &SentinelVmEntry;
		Binding.RawEntry = &SentinelRawEntry;
		Binding.ParmsEntry = &SentinelParmsEntry;
		Binding.UserData = UserData;
		return Binding;
	}

public:
	TEST_METHOD(PublishesCompleteBindingAfterCompilation)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("EMBED-JIT-COMPLETE-DELAYED-BINDING",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Complete JIT Binding lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("Complete JIT Binding lifecycle should retain JIT entry instructions")));

		FRecordingJitCompiler Compiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("Complete JIT Binding lifecycle should install one non-versioned compiler")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule ModuleScope(
			*TestRunner,
			Engine,
			"NativeJitCompleteBinding",
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Complete JIT Binding lifecycle should compile the source module")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptFunction* const EntryFunction = GetNativeFunctionByDecl(
			ModuleScope.Get(),
			"int Entry()");
		ASSERT_THAT(IsNotNull(
			EntryFunction,
			TEXT("Complete JIT Binding lifecycle should resolve the compiled entry")));
		if (EntryFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			Compiler.ReadyFunctions.Contains(EntryFunction),
			TEXT("Successful compilation should notify the installed compiler after the entry is ready")));
		const asSJITFunctionBinding InitialBinding = EntryFunction->GetJITBinding();
		ASSERT_THAT(IsNull(
			InitialBinding.VMEntry,
			TEXT("Readiness notification should not require immediate VM Binding publication")));
		ASSERT_THAT(IsNull(
			InitialBinding.RawEntry,
			TEXT("Readiness notification should not require immediate Raw Binding publication")));
		ASSERT_THAT(IsNull(
			InitialBinding.ParmsEntry,
			TEXT("Readiness notification should not require immediate Parms Binding publication")));
		ASSERT_THAT(IsNull(
			InitialBinding.UserData,
			TEXT("Readiness notification should not require immediate Binding user data")));

		int32 BindingUserData = 77;
		const asSJITFunctionBinding PublishedBinding = MakeSentinelBinding(&BindingUserData);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(PublishedBinding),
			TEXT("A ready function should accept delayed complete Binding publication")));

		const asSJITFunctionBinding ObservedBinding = EntryFunction->GetJITBinding();
		ASSERT_THAT(AreEqual(
			static_cast<asJITFunction>(&SentinelVmEntry),
			ObservedBinding.VMEntry,
			TEXT("Published Binding should expose the exact VM entry")));
		ASSERT_THAT(AreEqual(
			static_cast<asJITFunction_Raw>(&SentinelRawEntry),
			ObservedBinding.RawEntry,
			TEXT("Published Binding should expose the exact Raw entry")));
		ASSERT_THAT(AreEqual(
			static_cast<asJITFunction_ParmsEntry>(&SentinelParmsEntry),
			ObservedBinding.ParmsEntry,
			TEXT("Published Binding should expose the exact Parms entry")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&BindingUserData),
			ObservedBinding.UserData,
			TEXT("Published Binding should expose the exact opaque user data")));
	}

	TEST_METHOD(ReplacesAndClearsCompleteBindingsExactlyOnce)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Binding replacement lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FRecordingJitCompiler Compiler;
		Compiler.bReenterRelease = true;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("Binding replacement lifecycle should install its compiler")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		FScopedNativeModule ModuleScope(
			*TestRunner,
			Engine,
			"NativeJitBindingReplacement",
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Binding replacement lifecycle should compile its source module")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptFunction* const EntryFunction = GetNativeFunctionByDecl(
			ModuleScope.Get(),
			"int Entry()");
		ASSERT_THAT(IsNotNull(
			EntryFunction,
			TEXT("Binding replacement lifecycle should resolve the compiled entry")));
		if (EntryFunction == nullptr)
		{
			return;
		}

		int32 FirstUserData = 11;
		int32 SecondUserData = 22;
		const asSJITFunctionBinding FirstBinding = MakeSentinelBinding(&FirstUserData);
		const asSJITFunctionBinding SecondBinding = MakeSentinelBinding(&SecondUserData);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(FirstBinding),
			TEXT("Binding replacement lifecycle should publish the first complete Binding")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(SecondBinding),
			TEXT("Binding replacement lifecycle should replace the first complete Binding")));
		ASSERT_THAT(AreEqual(
			1,
			Compiler.ReleaseObservations.Num(),
			TEXT("Replacing one complete Binding should release the prior Binding exactly once")));
		if (Compiler.ReleaseObservations.Num() != 1)
		{
			return;
		}

		const FRecordingJitCompiler::FReleaseObservation& ReplacementRelease =
			Compiler.ReleaseObservations[0];
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&FirstUserData),
			ReplacementRelease.Binding.UserData,
			TEXT("Replacement should release the exact prior complete Binding")));
		ASSERT_THAT(IsTrue(
			ReplacementRelease.bObservedEmptyBinding,
			TEXT("Release callback should observe the retired Binding already absent")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ReplacementRelease.ReentrantExecuteResult,
			TEXT("Release callback should be able to re-enter the function through VM")));
		ASSERT_THAT(AreEqual(
			42,
			ReplacementRelease.ReentrantReturnValue,
			TEXT("Re-entrant execution during release should use the current VM body")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&SecondUserData),
			EntryFunction->GetJITBinding().UserData,
			TEXT("Replacement should publish the complete new Binding after retiring the old one")));

		asSJITFunctionBinding EmptyBinding;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(EmptyBinding),
			TEXT("Binding replacement lifecycle should explicitly clear the second Binding")));
		ASSERT_THAT(AreEqual(
			2,
			Compiler.ReleaseObservations.Num(),
			TEXT("Explicit clear should release the second Binding exactly once")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&SecondUserData),
			Compiler.ReleaseObservations[1].Binding.UserData,
			TEXT("Explicit clear should release the exact second complete Binding")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(EmptyBinding),
			TEXT("Clearing an already empty Binding should be a successful no-op")));
		ASSERT_THAT(AreEqual(
			2,
			Compiler.ReleaseObservations.Num(),
			TEXT("Clearing an already empty Binding should not duplicate release")));
	}

	TEST_METHOD(DetachedFunctionDestructionReleasesBindingExactlyOnce)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Detached function lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("Detached function lifecycle should retain JIT entry instructions")));

		FRecordingJitCompiler Compiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("Detached function lifecycle should install its compiler")));

		FScopedNativeModuleName ModuleScope(Engine, "NativeJitDetachedFunction");
		asIScriptModule* const Module = ScriptEngine->GetModule(
			ModuleScope.Get(),
			asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Detached function lifecycle should create an owner module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int DetachedEntry()
			{
				return 73;
			}
			)AS");
		asIScriptFunction* DetachedFunction = nullptr;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module->CompileFunction(
				"NativeJitDetachedFunction.as",
				ScriptSource.c_str(),
				0,
				0,
				&DetachedFunction),
			TEXT("Detached function lifecycle should compile an eligible detached function")));
		ASSERT_THAT(IsNotNull(
			DetachedFunction,
			TEXT("Detached function lifecycle should return the compiled function")));
		if (DetachedFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			Compiler.ReadyFunctions.Contains(DetachedFunction),
			TEXT("Successful detached compilation should notify the installed compiler")));
		int32 FunctionUserData = 29;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			DetachedFunction->SetJITBinding(MakeSentinelBinding(&FunctionUserData)),
			TEXT("Detached function lifecycle should publish its complete Binding")));
		asIScriptFunction* const ReleasedFunction = DetachedFunction;
		ASSERT_THAT(AreEqual(
			0,
			DetachedFunction->Release(),
			TEXT("Detached function lifecycle should consume the final function reference")));
		DetachedFunction = nullptr;

		ASSERT_THAT(AreEqual(
			1,
			Compiler.ReleaseObservations.Num(),
			TEXT("Detached function destruction should release its complete Binding exactly once")));
		if (Compiler.ReleaseObservations.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				ReleasedFunction,
				Compiler.ReleaseObservations[0].Function,
				TEXT("Function destruction should release the exact detached function Binding")));
			ASSERT_THAT(AreEqual(
				static_cast<void*>(&FunctionUserData),
				Compiler.ReleaseObservations[0].Binding.UserData,
				TEXT("Function destruction should release the exact complete Binding payload")));
		}
	}

	TEST_METHOD(BytecodeRestoreNotifiesOnlyEligibleFunctions)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Bytecode restore lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("Bytecode restore lifecycle should retain JIT entry instructions")));

		FRecordingJitCompiler Compiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("Bytecode restore lifecycle should install its compiler")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 91;
			}
			)AS");
		FMemoryBinaryStream Stream;
		{
			FScopedNativeModule SourceModule(
				*TestRunner,
				Engine,
				"NativeJitBytecodeSource",
				ScriptSource);
			ASSERT_THAT(IsTrue(
				SourceModule.IsValid(),
				TEXT("Bytecode restore lifecycle should compile its source module")));
			if (!SourceModule.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				SourceModule->SaveByteCode(&Stream, false),
				TEXT("Bytecode restore lifecycle should save eligible bytecode")));
		}

		Compiler.ReadyFunctions.Reset();
		Stream.ResetReadPosition();
		FScopedNativeModuleName RestoredModuleScope(Engine, "NativeJitBytecodeRestored");
		asIScriptModule* const RestoredModule = ScriptEngine->GetModule(
			RestoredModuleScope.Get(),
			asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(
			RestoredModule,
			TEXT("Bytecode restore lifecycle should create a destination module")));
		if (RestoredModule == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			RestoredModule->LoadByteCode(&Stream, nullptr),
			TEXT("Bytecode restore lifecycle should restore eligible bytecode")));
		asIScriptFunction* const RestoredEntry = RestoredModule->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(
			RestoredEntry,
			TEXT("Bytecode restore lifecycle should resolve its restored entry")));
		ASSERT_THAT(IsTrue(
			Compiler.ReadyFunctions.Contains(RestoredEntry),
			TEXT("Eligible bytecode restore should notify the installed compiler")));

		Compiler.ReadyFunctions.Reset();
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 0),
			TEXT("Ineligible function lifecycle should omit JIT entry instructions")));
		FScopedNativeModule IneligibleModule(
			*TestRunner,
			Engine,
			"NativeJitIneligible",
			ScriptSource);
		ASSERT_THAT(IsTrue(
			IneligibleModule.IsValid(),
			TEXT("Ineligible function lifecycle should still compile for VM")));
		ASSERT_THAT(AreEqual(
			0,
			Compiler.ReadyFunctions.Num(),
			TEXT("A function without JIT entry instructions should not notify the compiler")));
	}

	TEST_METHOD(ModuleDiscardAndEngineShutdownReleaseBindings)
	{
		using namespace AngelscriptNativeTestSupport;

		FRecordingJitCompiler Compiler;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Binding teardown lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&Compiler),
			TEXT("Binding teardown lifecycle should install its compiler")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 64;
			}
			)AS");
		int32 ModuleUserData = 31;
		{
			FScopedNativeModule ModuleScope(
				*TestRunner,
				Engine,
				"NativeJitBindingModuleDiscard",
				ScriptSource);
			ASSERT_THAT(IsTrue(
				ModuleScope.IsValid(),
				TEXT("Binding teardown lifecycle should compile its discard module")));
			if (!ModuleScope.IsValid())
			{
				Engine.Destroy();
				return;
			}

			asIScriptFunction* const EntryFunction = GetNativeFunctionByDecl(
				ModuleScope.Get(),
				"int Entry()");
			ASSERT_THAT(IsNotNull(
				EntryFunction,
				TEXT("Binding teardown lifecycle should resolve the discard entry")));
			if (EntryFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					EntryFunction->SetJITBinding(MakeSentinelBinding(&ModuleUserData)),
					TEXT("Binding teardown lifecycle should publish the module-owned Binding")));
			}
		}

		ASSERT_THAT(AreEqual(
			1,
			Compiler.ReleaseObservations.Num(),
			TEXT("Module discard should release its complete Binding exactly once")));
		if (Compiler.ReleaseObservations.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				static_cast<void*>(&ModuleUserData),
				Compiler.ReleaseObservations[0].Binding.UserData,
				TEXT("Module discard should release the exact module-owned Binding")));
		}

		int32 EngineUserData = 32;
		FScopedNativeModule EngineShutdownModule(
			*TestRunner,
			Engine,
			"NativeJitBindingEngineShutdown",
			ScriptSource);
		ASSERT_THAT(IsTrue(
			EngineShutdownModule.IsValid(),
			TEXT("Binding teardown lifecycle should compile its engine-shutdown module")));
		asIScriptFunction* const ShutdownEntry = GetNativeFunctionByDecl(
			EngineShutdownModule.Get(),
			"int Entry()");
		ASSERT_THAT(IsNotNull(
			ShutdownEntry,
			TEXT("Binding teardown lifecycle should resolve the engine-shutdown entry")));
		if (ShutdownEntry != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				ShutdownEntry->SetJITBinding(MakeSentinelBinding(&EngineUserData)),
				TEXT("Binding teardown lifecycle should publish the engine-owned Binding")));
		}

		Engine.Destroy();
		ASSERT_THAT(AreEqual(
			2,
			Compiler.ReleaseObservations.Num(),
			TEXT("Engine shutdown should release its remaining complete Binding exactly once")));
		if (Compiler.ReleaseObservations.Num() == 2)
		{
			ASSERT_THAT(AreEqual(
				static_cast<void*>(&EngineUserData),
				Compiler.ReleaseObservations[1].Binding.UserData,
				TEXT("Engine shutdown should release the exact remaining Binding")));
		}
	}

	TEST_METHOD(CompilerReplacementAndRemovalTransferLifecycleOwnership)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Compiler transfer lifecycle should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("Compiler transfer lifecycle should retain JIT entry instructions")));

		FRecordingJitCompiler PrimaryCompiler;
		FRecordingJitCompiler ReplacementCompiler;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&PrimaryCompiler),
			TEXT("Compiler transfer lifecycle should install the primary compiler")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 81;
			}
			)AS");
		FScopedNativeModule ModuleScope(
			*TestRunner,
			Engine,
			"NativeJitCompilerTransfer",
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Compiler transfer lifecycle should compile its source module")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptFunction* const EntryFunction = GetNativeFunctionByDecl(
			ModuleScope.Get(),
			"int Entry()");
		ASSERT_THAT(IsNotNull(
			EntryFunction,
			TEXT("Compiler transfer lifecycle should resolve the compiled entry")));
		if (EntryFunction == nullptr)
		{
			return;
		}

		int32 PrimaryUserData = 41;
		int32 ReplacementUserData = 42;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(MakeSentinelBinding(&PrimaryUserData)),
			TEXT("Compiler transfer lifecycle should publish the primary Binding")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(&ReplacementCompiler),
			TEXT("Compiler transfer lifecycle should replace the installed compiler")));
		ASSERT_THAT(AreEqual(
			1,
			PrimaryCompiler.ReleaseObservations.Num(),
			TEXT("Compiler replacement should release the primary Binding exactly once")));
		ASSERT_THAT(IsTrue(
			ReplacementCompiler.ReadyFunctions.Contains(EntryFunction),
			TEXT("Compiler replacement should notify the new compiler about the existing eligible function")));
		ASSERT_THAT(IsNull(
			EntryFunction->GetJITBinding().VMEntry,
			TEXT("Compiler replacement should leave the function on VM until the new compiler publishes")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			EntryFunction->SetJITBinding(MakeSentinelBinding(&ReplacementUserData)),
			TEXT("Compiler transfer lifecycle should publish the replacement Binding")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(nullptr),
			TEXT("Compiler transfer lifecycle should remove the replacement compiler")));
		ASSERT_THAT(AreEqual(
			1,
			ReplacementCompiler.ReleaseObservations.Num(),
			TEXT("Compiler removal should release the replacement Binding exactly once")));
		ASSERT_THAT(IsNull(
			EntryFunction->GetJITBinding().VMEntry,
			TEXT("Compiler removal should leave the current function executable through VM")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetJITCompiler(nullptr),
			TEXT("Removing an already absent compiler should be a successful no-op")));
		ASSERT_THAT(AreEqual(
			1,
			ReplacementCompiler.ReleaseObservations.Num(),
			TEXT("Removing an already absent compiler should not duplicate release")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
