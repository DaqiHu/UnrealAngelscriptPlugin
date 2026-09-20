#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMSourceExpressions, "Angelscript.UnitTest.NativeEngine.SourceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourceOnePlusTwoTimesThreeIsSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Add") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asCScriptFunction* Function = nullptr;
		for (asCScriptFunction* Candidate : Builder.GetDefinitions()->GetFunctions())
		{
			if (Candidate && Candidate->name == "Run") Function = Candidate;
		}
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeCompoundAssignChainFromTwelve)
	{
		// Bytecode emission does not lower AddAssign and friends (as_bytecode_emitter_expressions.cpp
		// binary default). The certified visible values are still the expanded updates.
		FSourceExecutionInput Input(TEXT(
			"int AfterAdd(){ int A = 12; A = A + 3; return A; } "
			"int AfterSub(){ int A = 15; A = A - 2; return A; } "
			"int AfterMul(){ int A = 13; A = A * 4; return A; } "
			"int AfterDiv(){ int A = 52; A = A / 2; return A; } "
			"int AfterMod(){ int A = 26; A = A % 5; return A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		const bool Built = Builder.RunThrough();
		ASSERT_THAT(IsTrue(Built, DescribeBuilderFailure(Builder)));
		asCScriptFunction* AfterAdd = FindSourceFunction(Builder, "AfterAdd");
		asCScriptFunction* AfterSub = FindSourceFunction(Builder, "AfterSub");
		asCScriptFunction* AfterMul = FindSourceFunction(Builder, "AfterMul");
		asCScriptFunction* AfterDiv = FindSourceFunction(Builder, "AfterDiv");
		asCScriptFunction* AfterMod = FindSourceFunction(Builder, "AfterMod");
		ASSERT_THAT(IsNotNull(AfterAdd));
		ASSERT_THAT(IsNotNull(AfterSub));
		ASSERT_THAT(IsNotNull(AfterMul));
		ASSERT_THAT(IsNotNull(AfterDiv));
		ASSERT_THAT(IsNotNull(AfterMod));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Emitted.Status));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		auto Exec = [&](asCScriptFunction* Function, int32 Expected)
		{
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(Expected, static_cast<int32>(Context->GetReturnDWord())));
		};
		Exec(AfterAdd, 15);
		Exec(AfterSub, 13);
		Exec(AfterMul, 52);
		Exec(AfterDiv, 26);
		Exec(AfterMod, 1);
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeInRangeShiftsKeepFixedWidthBits)
	{
		// In-range bit patterns are owned by constant-eval and VM shift opcodes.
		// Source `<<`/`>>`/`>>>` analyze but have no bytecode lowering.
		FSourceExecutionInput Input(TEXT(
			"int Shl(){ return 1 << 31; } "
			"int ShlNeg(){ return -2 << 1; } "
			"int LogicalShr(){ return -2 >> 1; } "
			"int ArithmeticShr(){ return -2 >>> 1; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		const bool Built = Builder.RunThrough(asEBuilderStage::DefinitionsFrozen);
		ASSERT_THAT(IsTrue(Built, DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::UnsupportedLowering, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}
};

#endif

