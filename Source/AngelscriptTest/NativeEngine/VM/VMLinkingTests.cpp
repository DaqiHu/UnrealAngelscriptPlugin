#include "CQTest.h"
#include "NativeEngine/SourceExecution/NativeSourceExecutionTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMLinking, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LinkResolvesOriginalFunctionPointers)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder, true)));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindFunction(Function->GetStableKey()) == Function));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
	}

	TEST_METHOD(MissingDeclarationAndBodyConflictReject)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1; } class Dup { int V; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FNativeEngineOwner Engine;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreNotEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder, true)));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		FSourceExecutionInput SecondInput(TEXT("class Dup { int V; }"));
		asCBuilder Second(SecondInput.Sources, SecondInput.Options);
		ASSERT_THAT(IsTrue(Second.RunThrough(), DescribeBuilderFailure(Second)));
		ASSERT_THAT(AreEqual(asERegistrationResult::IdentityConflict, RegisterCompiledDefinitions(*Engine.Ptr, Second)));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
	}
};

#endif
