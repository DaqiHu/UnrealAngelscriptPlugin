#include "CQTest.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMImageContracts, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RegistrationWritesFunctionRuntime)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 7; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		asUINT Before = 0;
		ASSERT_THAT(IsNull(Function->GetByteCode(&Before)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder, true)));
		asUINT After = 0;
		ASSERT_THAT(IsNotNull(Function->GetByteCode(&After)));
		ASSERT_THAT(IsTrue(After > 0));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
	}

	TEST_METHOD(UnregisteredScriptFunctionHasNoRuntime)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FNativeEngineOwner Engine;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(asINVALID_ARG, Context->Prepare(Function)));
		Context->Release();
	}

	TEST_METHOD(SameNameDifferentReturnsKeepDistinctRuntime)
	{
		FSourceExecutionInput IntInput(TEXT("int Read() { return 1; }"));
		asCBuilder IntBuilder(IntInput.Sources, IntInput.Options);
		ASSERT_THAT(IsTrue(IntBuilder.RunThrough(), DescribeBuilderFailure(IntBuilder)));
		asCScriptFunction* Left = FindSourceFunction(IntBuilder, "Read");
		FSourceExecutionInput DoubleInput(TEXT("double Read() { return 1.5; }"));
		asCBuilder DoubleBuilder(DoubleInput.Sources, DoubleInput.Options);
		ASSERT_THAT(IsTrue(DoubleBuilder.RunThrough(), DescribeBuilderFailure(DoubleBuilder)));
		asCScriptFunction* Right = FindSourceFunction(DoubleBuilder, "Read");
		ASSERT_THAT(IsNotNull(Left));
		ASSERT_THAT(IsNotNull(Right));
		ASSERT_THAT(IsTrue(Left != Right));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, IntBuilder, true)));
		ASSERT_THAT(AreEqual(asERegistrationResult::IdentityConflict, RegisterCompiledDefinitions(*Engine.Ptr, DoubleBuilder)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Left)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
	}
};

#endif
