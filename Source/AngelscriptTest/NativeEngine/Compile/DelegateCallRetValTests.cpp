#include "CQTest.h"

#include "NativeEngine/NativeEngineTestSupport.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "as_builder.h"
#include "as_bytecode_linker.h"
#include "as_compile_output.h"
#include "as_context.h"
#include "as_engine_compile_registration.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateCallRetValTests
{
	asCScriptFunction* FindNamed(asCDefinitions& Set, const char* Name)
	{
		asCScriptFunction* Fallback = nullptr;
		for (asCScriptFunction* Function : Set.GetFunctions())
		{
			if (!Function || Function->name != Name) continue;
			if (!Function->objectType) return Function;
			Fallback = Function;
		}
		return Fallback;
	}

	bool CompileInvoke(TUniquePtr<asCDefinitions>& OutSet, asCScriptFunction*& OutAddOne, asCScriptFunction*& OutInvoke, FString& OutError)
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("CallPtr.as"),
				TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FAdd, int);\n")
				TEXT("int AddOne(int Value) { return Value + 1; }\n")
				TEXT("int Invoke(FAdd Handler, int Value) { return Handler(Value); }\n"));
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("delegate-call-retval");
		Options.TypeContext = TypeContext;
		asCBuilder Builder(Sources, Options);
		if (!Builder.RunThrough())
		{
			OutError = DescribeBuilderFailure(Builder);
			return false;
		}
		OutSet = Builder.TakeDefinitions();
		if (!OutSet.Get())
		{
			OutError = TEXT("missing definitions");
			return false;
		}
		OutAddOne = FindNamed(*OutSet, "AddOne");
		OutInvoke = FindNamed(*OutSet, "Invoke");
		if (!OutAddOne || !OutInvoke)
		{
			OutError = TEXT("missing AddOne or Invoke");
			return false;
		}
		return true;
	}

	asERegistrationResult RegisterCompiled(asCScriptEngine& Engine, TUniquePtr<asCDefinitions> Set, FString& OutError)
	{
		if (!Set.Get())
		{
			OutError = TEXT("missing definitions");
			return asERegistrationResult::InvalidArgument;
		}
		if (!Set->ValidateFrozenDefinitions())
		{
			OutError = TEXT("invalid-frozen-definitions");
			return asERegistrationResult::InvalidLayout;
		}
		TUniquePtr<asCByteCodeImage> ByteCode = Set->TakeEmittedByteCode();
		asCEngineCompileRegistration Registration(Engine);
		TArray<TUniquePtr<asCDefinitions>> Units;
		Units.Add(MoveTemp(Set));
		const asERegistrationResult Installed = Registration.Register(MoveTemp(Units));
		if (Installed != asERegistrationResult::Succeeded)
		{
			OutError = FString::Printf(TEXT("register=%d"), int32(Installed));
			return Installed;
		}
		if (!ByteCode.Get())
		{
			return asERegistrationResult::Succeeded;
		}
		asCExecutableSnapshot* Snapshot = nullptr;
		const asEByteCodeLinkStatus Linked = asLinkByteCodeImage(Engine, *ByteCode, Snapshot);
		if (Snapshot)
		{
			Snapshot->Release();
		}
		if (Linked != asEByteCodeLinkStatus::Succeeded)
		{
			OutError = FString::Printf(TEXT("link=%d"), int32(Linked));
			return asERegistrationResult::InvalidLayout;
		}
		return asERegistrationResult::Succeeded;
	}
}

TEST_CLASS_WITH_FLAGS(DelegateCallRetVal, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CallPtrReturnsFortyTwo)
	{
		TUniquePtr<asCDefinitions> Set;
		asCScriptFunction* AddOne = nullptr;
		asCScriptFunction* Invoke = nullptr;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateCallRetValTests::CompileInvoke(Set, AddOne, Invoke, Error), *Error));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, DelegateCallRetValTests::RegisterCompiled(*Engine, MoveTemp(Set), Error), *Error));
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Invoke) >= 0));
		Context->SetArgAddress(0, AddOne);
		Context->SetArgDWord(1, 41);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
	}

	TEST_METHOD(NullCallableThrows)
	{
		TUniquePtr<asCDefinitions> Set;
		asCScriptFunction* AddOne = nullptr;
		asCScriptFunction* Invoke = nullptr;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateCallRetValTests::CompileInvoke(Set, AddOne, Invoke, Error), *Error));
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		ON_SCOPE_EXIT { Engine->ShutDownAndRelease(); };
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, DelegateCallRetValTests::RegisterCompiled(*Engine, MoveTemp(Set), Error), *Error));
		asIScriptContext* Context = Engine->CreateContext();
		ON_SCOPE_EXIT { if (Context) Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Invoke) >= 0));
		Context->SetArgAddress(0, nullptr);
		Context->SetArgDWord(1, 41);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
	}
};

#endif
