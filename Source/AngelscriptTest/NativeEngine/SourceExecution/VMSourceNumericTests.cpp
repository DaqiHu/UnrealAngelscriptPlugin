#include "CQTest.h"
#include "NativeSourceExecutionTestSupport.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMSourceNumericPrivate
{
	bool FunctionHasOpcode(const asCByteCodeImage& Image, asEBCInstr Opcode)
	{
		for (const asSByteCodeFunctionBody& Body : Image.GetFunctions())
		{
			for (const asSByteCodeInstruction& Instruction : Body.Instructions)
			{
				if (Instruction.Opcode == Opcode)
				{
					return true;
				}
			}
		}
		return false;
	}

	asUINT MaxFrameSize(const asCByteCodeImage& Image)
	{
		asUINT Max = 0;
		for (const asSByteCodeFunctionBody& Body : Image.GetFunctions())
		{
			Max = FMath::Max(Max, Body.FrameSize);
		}
		return Max;
	}
}

TEST_CLASS_WITH_FLAGS(VMSourceNumeric, "Angelscript.UnitTest.NativeEngine.SourceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DoubleSubtractLiteralEmitsSubdNotAddd)
	{
		FSourceExecutionInput Input(TEXT("double Run() { return 8.5-2.0; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_SUBd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(DoubleMultiplyLiteralEmitsMuldNotAddd)
	{
		FSourceExecutionInput Input(TEXT("double Run() { return 2.5*4.0; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_MULd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(10.0 == Context->GetReturnDouble()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(DoubleDivideLiteralEmitsDivd)
	{
		FSourceExecutionInput Input(TEXT("double Run() { return 9.0/2.0; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_DIVd)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(4.5 == Context->GetReturnDouble()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(DoubleOpsThroughParameterLocalCastAndConditional)
	{
		FSourceExecutionInput Input(TEXT(
			"double FromParams(double A, double B) { return A-B; } "
			"double FromLocals() { double A = 8.5; double B = 2.0; return A-B; } "
			"double FromCast() { return double(17)/2.0 - 2.0; } "
			"double FromConditional(bool UseSub) { return UseSub ? 8.5-2.0 : 2.5*4.0; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_SUBd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		asCScriptFunction* FromParams = FindSourceFunction(Builder, "FromParams");
		asCScriptFunction* FromLocals = FindSourceFunction(Builder, "FromLocals");
		asCScriptFunction* FromCast = FindSourceFunction(Builder, "FromCast");
		asCScriptFunction* FromConditional = FindSourceFunction(Builder, "FromConditional");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromParams)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(0, 8.5)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(1, 2.0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromLocals)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromCast)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromConditional)));
		ASSERT_THAT(AreEqual(0, Context->SetArgByte(0, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromConditional)));
		ASSERT_THAT(AreEqual(0, Context->SetArgByte(0, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(10.0 == Context->GetReturnDouble()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FloatCompareAssignAndNonIntCast)
	{
		FSourceExecutionInput Input(TEXT(
			"bool Greater() { float A = 4.0; float B = 1.5; return A > B; } "
			"double Widen() { float A = 8.5; return A; } "
			"int Truncate() { return int(9.0 / 2.0); }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		asCScriptFunction* Greater = FindSourceFunction(Builder, "Greater");
		asCScriptFunction* Widen = FindSourceFunction(Builder, "Widen");
		asCScriptFunction* Truncate = FindSourceFunction(Builder, "Truncate");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Greater)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Widen)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(8.5 == Context->GetReturnDouble()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Truncate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(Int64Above32BitsPreserved)
	{
		FSourceExecutionInput Input(TEXT("int64 Run() { int64 A = 4294967296; return A + 1; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDi64)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(4294967297LL == static_cast<int64>(Context->GetReturnQWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(UnsignedHighBitCompareAndDivide)
	{
		FSourceExecutionInput Input(TEXT(
			"bool HighGreater() { uint64 A = 9223372036854775808; return A > uint64(1); } "
			"uint64 HighDiv() { uint64 A = 9223372036854775808; return A / uint64(2); }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_CMPu64)
			|| VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_DIVu64)));
		asCScriptFunction* Greater = FindSourceFunction(Builder, "HighGreater");
		asCScriptFunction* Div = FindSourceFunction(Builder, "HighDiv");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Greater)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Div)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(4611686018427387904ULL == Context->GetReturnQWord()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NarrowSignExtension)
	{
		FSourceExecutionInput Input(TEXT("int FromInt8() { int8 X = -1; return X; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		asCScriptFunction* Function = FindSourceFunction(Builder, "FromInt8");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(-1, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(BoolOrPreserved)
	{
		FSourceExecutionInput Input(TEXT("bool Flag() { return true || false; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Flag");
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

	TEST_METHOD(EnumValuePreserved)
	{
		FSourceExecutionInput Input(TEXT("enum Shade { Dark = 3 } int EnumValue() { return Shade::Dark; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		asCScriptFunction* Function = FindSourceFunction(Builder, "EnumValue");
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

	TEST_METHOD(IntegerControlStillSeven)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 1+2*3; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDi)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
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

	TEST_METHOD(FrameAbove64DwordsExecutesWhenBudgetAllows)
	{
		FString Source = TEXT("int Run() { ");
		for (int32 Index = 0; Index < 40; ++Index)
		{
			Source += FString::Printf(TEXT("int V%d = %d; "), Index, Index);
		}
		Source += TEXT("return V0 + V39; }");
		FSourceExecutionInput Input(Source);
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(asEBuilderStage::DefinitionsFrozen), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionOptions Options;
		Options.MaxFrameDWords = 256;
		FSessionEmission Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::MaxFrameSize(*Emitted.Image) > 64));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(39, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(OverBudgetFrameRejectsWithoutImage)
	{
		FString Source = TEXT("int Run() { ");
		for (int32 Index = 0; Index < 40; ++Index)
		{
			Source += FString::Printf(TEXT("int V%d = %d; "), Index, Index);
		}
		Source += TEXT("return V0 + V39; }");
		FSourceExecutionInput Input(Source);
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(asEBuilderStage::DefinitionsFrozen), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionOptions Options;
		Options.MaxFrameDWords = 16;
		FSessionEmission Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::ResourceLimit, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}

	TEST_METHOD(RuntimeIntSevenAndThreeArithmetic)
	{
		FSourceExecutionInput Input(TEXT(
			"int Add(int A, int B){ return A+B; } "
			"int Sub(int A, int B){ return A-B; } "
			"int Mul(int A, int B){ return A*B; } "
			"int Div(int A, int B){ return A/B; } "
			"int Mod(int A, int B){ return A%B; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Add = FindSourceFunction(Builder, "Add");
		asCScriptFunction* Sub = FindSourceFunction(Builder, "Sub");
		asCScriptFunction* Mul = FindSourceFunction(Builder, "Mul");
		asCScriptFunction* Div = FindSourceFunction(Builder, "Div");
		asCScriptFunction* Mod = FindSourceFunction(Builder, "Mod");
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(IsNotNull(Sub));
		ASSERT_THAT(IsNotNull(Mul));
		ASSERT_THAT(IsNotNull(Div));
		ASSERT_THAT(IsNotNull(Mod));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		auto Exec = [&](asCScriptFunction* Function, int32 Expected)
		{
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 3)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(Expected, static_cast<int32>(Context->GetReturnDWord())));
		};
		Exec(Add, 10);
		Exec(Sub, 4);
		Exec(Mul, 21);
		Exec(Div, 2);
		Exec(Mod, 1);
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeFloatSevenPointFiveAndTwo)
	{
		FSourceExecutionInput Input(TEXT(
			"double Add(double A, double B){ return A+B; } "
			"double Sub(double A, double B){ return A-B; } "
			"double Mul(double A, double B){ return A*B; } "
			"double Div(double A, double B){ return A/B; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Add = FindSourceFunction(Builder, "Add");
		asCScriptFunction* Sub = FindSourceFunction(Builder, "Sub");
		asCScriptFunction* Mul = FindSourceFunction(Builder, "Mul");
		asCScriptFunction* Div = FindSourceFunction(Builder, "Div");
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(IsNotNull(Sub));
		ASSERT_THAT(IsNotNull(Mul));
		ASSERT_THAT(IsNotNull(Div));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		auto Exec = [&](asCScriptFunction* Function, double Expected)
		{
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDouble(0, 7.5)));
			ASSERT_THAT(AreEqual(0, Context->SetArgDouble(1, 2.0)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(IsTrue(Expected == Context->GetReturnDouble()));
		};
		Exec(Add, 9.5);
		Exec(Sub, 5.5);
		Exec(Mul, 15.0);
		Exec(Div, 3.75);
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeIntDivideByZeroIsException)
	{
		FSourceExecutionInput Input(TEXT("int Run(int A, int B){ return A/B; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 0)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "Divide by zero") != nullptr));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 1)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeMinInt32DivNegOneOverflows)
	{
		FSourceExecutionInput Input(TEXT("int Run(int A, int B){ return A/B; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, static_cast<asDWORD>(0x80000000u))));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, static_cast<asDWORD>(-1))));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "Overflow in integer division") != nullptr));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeSignedAddWrapsAtInt32)
	{
		FSourceExecutionInput Input(TEXT("int Run(){ int A = 2147483647; return A + 1; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(static_cast<int32>(0x80000000), static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeDoubleDivideByZeroIsException)
	{
		FSourceExecutionInput Input(TEXT("double Run(double A, double B){ return A/B; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(0, 7.5)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(1, 0.0)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Context->GetExceptionString(), "Divide by zero") != nullptr));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RuntimeIntWidensToDoubleSeven)
	{
		FSourceExecutionInput Input(TEXT("double Run(int A){ return A; }"));
		asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		FSessionEmission Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, Builder)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(7.0 == Context->GetReturnDouble()));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
