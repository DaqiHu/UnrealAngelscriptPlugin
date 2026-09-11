#include "StaticJIT/AngelscriptJITExecutionContext.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Shared/AngelscriptTestMacros.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITExecutionContextTests_Private
{
	static const FAngelscriptJITResolvedReferenceTable* ExpectedReferences = nullptr;
	static const asSJITFunctionBinding* ExpectedBinding = nullptr;
	static asIScriptFunction* ExpectedFunction = nullptr;
	static int32 VMCalls = 0;
	static int32 RawCalls = 0;
	static int32 ParmsCalls = 0;
	static const FScriptExecution* ActiveRawExecution = nullptr;
	static asIScriptEngine* ExpectedEngine = nullptr;
	static void* ExpectedUserData = nullptr;
	static bool bObservedCoreVmContext = false;

	static bool HasExpectedContext(const FScriptExecution& Execution)
	{
		return Execution.jitFunction == ExpectedFunction
			&& Execution.jitBinding == ExpectedBinding
			&& Execution.jitBindingUserData == ExpectedBinding->UserData
			&& Execution.jitReferenceTable == ExpectedReferences;
	}

	static void VMEntry(FScriptExecution& Execution, asDWORD*, asQWORD*)
	{
		if (HasExpectedContext(Execution))
		{
			++VMCalls;
		}
	}

	static void RawEntry()
	{
		if (ActiveRawExecution != nullptr
			&& HasExpectedContext(*ActiveRawExecution))
		{
			++RawCalls;
		}
	}

	static void ParmsEntry(FScriptExecution& Execution, void*, void*)
	{
		if (HasExpectedContext(Execution))
		{
			++ParmsCalls;
		}
	}

	static void CoreVmEntry(
		FScriptExecution& Execution,
		asDWORD*,
		asQWORD* OutValue)
	{
		bObservedCoreVmContext = Execution.jitEngine == ExpectedEngine
			&& Execution.jitFunction == ExpectedFunction
			&& Execution.jitBinding != nullptr
			&& Execution.jitBinding->VMEntry == &CoreVmEntry
			&& Execution.jitBinding->UserData == ExpectedUserData
			&& Execution.jitBindingUserData == ExpectedUserData;
		if (OutValue != nullptr)
		{
			*OutValue = 77;
		}
	}

	class FCompiler final : public asIJITCompiler
	{
	public:
		void OnFunctionReady(asIScriptFunction*) override
		{
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding&) override
		{
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITExecutionContextTests,
	"Angelscript.TestModule.StaticJIT.ExecutionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(VMRawAndParmsObserveOneCompleteExplicitBindingContext)
	{
		using namespace AngelscriptJITExecutionContextTests_Private;
		VMCalls = RawCalls = ParmsCalls = 0;
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		FAngelscriptJITResolvedReferenceTable References;
		int UserData = 42;
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &VMEntry;
		Binding.RawEntry = &RawEntry;
		Binding.ParmsEntry = &ParmsEntry;
		Binding.UserData = &UserData;
		ExpectedReferences = &References;
		ExpectedBinding = &Binding;
		ExpectedFunction = reinterpret_cast<asIScriptFunction*>(UPTRINT(0x1234));

		FAngelscriptJITExecutionContext::InvokeVM(
			Execution, nullptr, ExpectedFunction, Binding, References, nullptr, nullptr);
		FAngelscriptJITExecutionContext::InvokeRaw(
			Execution, nullptr, ExpectedFunction, Binding, References,
			[](asJITFunction_Raw Entry, FScriptExecution& ScopedExecution)
			{
				ActiveRawExecution = &ScopedExecution;
				Entry();
				ActiveRawExecution = nullptr;
			});
		FAngelscriptJITExecutionContext::InvokeParms(
			Execution, nullptr, ExpectedFunction, Binding, References, nullptr, nullptr);

		ASSERT_THAT(AreEqual(1, VMCalls));
		ASSERT_THAT(AreEqual(1, RawCalls));
		ASSERT_THAT(AreEqual(1, ParmsCalls));
		ASSERT_THAT(IsNull(Execution.jitFunction));
		ASSERT_THAT(IsNull(Execution.jitBinding));
		ASSERT_THAT(IsNull(Execution.jitBindingUserData));
		ASSERT_THAT(IsNull(Execution.jitReferenceTable));
	}

	TEST_METHOD(ReentrantScopeRestoresOuterBindingAndReferences)
	{
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		FAngelscriptJITResolvedReferenceTable OuterReferences;
		FAngelscriptJITResolvedReferenceTable InnerReferences;
		asSJITFunctionBinding OuterBinding;
		asSJITFunctionBinding InnerBinding;
		asIScriptFunction* const OuterFunction =
			reinterpret_cast<asIScriptFunction*>(UPTRINT(0x1000));
		asIScriptFunction* const InnerFunction =
			reinterpret_cast<asIScriptFunction*>(UPTRINT(0x2000));

		FAngelscriptJITExecutionScope Outer(
			Execution, nullptr, OuterFunction, OuterBinding, OuterReferences);
		{
			FAngelscriptJITExecutionScope Inner(
				Execution, nullptr, InnerFunction, InnerBinding, InnerReferences);
			ASSERT_THAT(IsTrue(Execution.jitFunction == InnerFunction));
			ASSERT_THAT(IsTrue(Execution.jitReferenceTable == &InnerReferences));
		}
		ASSERT_THAT(IsTrue(Execution.jitFunction == OuterFunction));
		ASSERT_THAT(IsTrue(Execution.jitBinding == &OuterBinding));
		ASSERT_THAT(IsTrue(Execution.jitReferenceTable == &OuterReferences));
	}

	TEST_METHOD(MissingEntryNeverPublishesPartialExecutionContext)
	{
		FScriptExecution Execution(asCThreadManager::GetLocalData());
		FAngelscriptJITResolvedReferenceTable References;
		asSJITFunctionBinding EmptyBinding;
		const bool bInvoked = FAngelscriptJITExecutionContext::InvokeVM(
			Execution, nullptr, nullptr, EmptyBinding, References, nullptr, nullptr);
		ASSERT_THAT(IsFalse(bInvoked));
		ASSERT_THAT(IsNull(Execution.jitBinding));
		ASSERT_THAT(IsNull(Execution.jitReferenceTable));
	}

	TEST_METHOD(ArbitraryLegacyUserDataCannotMasqueradeAsProviderContext)
	{
		// A legacy compiler owns UserData and may store any payload there. Even a
		// byte-for-byte magic collision must not make Runtime treat that payload as
		// an FAngelscriptJITBindingContext and dereference unrelated memory.
		uint64 LegacyPayload = 0x41534A4954424E44ull;
		ASSERT_THAT(IsNull(
			FAngelscriptJITBindingContext::FromBindingUserData(&LegacyPayload)));
	}

	TEST_METHOD(CoreVmDispatchPublishesCurrentEngineFunctionAndCompleteBinding)
	{
		using namespace AngelscriptJITExecutionContextTests_Private;
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
		FCompiler Compiler;
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&Compiler)));
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
			*TestRunner, Engine, "JitExecutionContextCoreVm", Source);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(
			Module.Get(), "int Entry()");
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		int32 UserData = 123;
		asSJITFunctionBinding Binding;
		Binding.VMEntry = &CoreVmEntry;
		Binding.UserData = &UserData;
		ASSERT_THAT(AreEqual(asSUCCESS, Function->SetJITBinding(Binding)));

		ExpectedEngine = ScriptEngine;
		ExpectedFunction = Function;
		ExpectedUserData = &UserData;
		bObservedCoreVmContext = false;
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(77u, Context->GetReturnDWord()));
		ASSERT_THAT(IsTrue(bObservedCoreVmContext));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
