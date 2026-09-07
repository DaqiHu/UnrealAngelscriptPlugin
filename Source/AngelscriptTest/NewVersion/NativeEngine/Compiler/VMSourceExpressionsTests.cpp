#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMSourceExpressions, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceOnePlusTwoTimesThreeIsSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceEightMinusThreeMinusOneIsFour)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 8-3-1; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourcePowerIsLeftAssociatedSixtyFour)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 2**3**2; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(64, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceBitwiseOrEqualsIsTrue)
	{
		FSourceExecutionInput Input(TEXT("bool Run() { return 1|2==3; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceOnePlusTwoPointFiveIsThreePointFive)
	{
		FSourceExecutionInput Input(TEXT("double Run() { return 1+2.5; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsNear(3.5, Context->GetReturnDouble(), 0.0000001));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceAddWithRuntimeParameters)
	{
		FSourceExecutionInput Input(TEXT("int Add(int a, int b) { return a+b; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Add") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 35)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SourceUnaryMinusAndLocalAssignment)
	{
		FSourceExecutionInput Input(TEXT("int Run() { int x = 4; x = x + 1; return -x + 8; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		asSByteCodeEmissionResult Emitted = asCByteCodeEmitter::Emit(Builder, *Function);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.GetDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif

