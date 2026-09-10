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

TEST_CLASS_WITH_FLAGS(VMSourceNumeric, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DoubleSubtractLiteralEmitsSubdNotAddd)
	{
		FSourceExecutionInput Input(TEXT("double Run() { return 8.5-2.0; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsNotNull(Emitted.Image.Get()));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_SUBd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_MULd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_DIVd)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_SUBd)));
		ASSERT_THAT(IsFalse(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDd)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		asCScriptFunction* FromParams = FindSourceFunction(Builder, "FromParams");
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromParams)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(0, 8.5)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDouble(1, 2.0)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		asCScriptFunction* FromLocals = FindSourceFunction(Builder, "FromLocals");
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromLocals)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		asCScriptFunction* FromCast = FindSourceFunction(Builder, "FromCast");
		ASSERT_THAT(AreEqual(0, Context->Prepare(FromCast)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(6.5 == Context->GetReturnDouble()));
		asCScriptFunction* FromConditional = FindSourceFunction(Builder, "FromConditional");
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		asCScriptFunction* Greater = FindSourceFunction(Builder, "Greater");
		ASSERT_THAT(AreEqual(0, Context->Prepare(Greater)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		asCScriptFunction* Widen = FindSourceFunction(Builder, "Widen");
		ASSERT_THAT(AreEqual(0, Context->Prepare(Widen)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(8.5 == Context->GetReturnDouble()));
		asCScriptFunction* Truncate = FindSourceFunction(Builder, "Truncate");
		ASSERT_THAT(AreEqual(0, Context->Prepare(Truncate)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(Int64Above32BitsPreserved)
	{
		FSourceExecutionInput Input(TEXT("int64 Run() { int64 A = 4294967296; return A + 1; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDi64)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_CMPu64)
			|| VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_DIVu64)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		asCScriptFunction* Greater = FindSourceFunction(Builder, "HighGreater");
		ASSERT_THAT(AreEqual(0, Context->Prepare(Greater)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnByte())));
		asCScriptFunction* Div = FindSourceFunction(Builder, "HighDiv");
		ASSERT_THAT(AreEqual(0, Context->Prepare(Div)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(IsTrue(4611686018427387904ULL == Context->GetReturnQWord()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NarrowSignExtension)
	{
		FSourceExecutionInput Input(TEXT("int FromInt8() { int8 X = -1; return X; }"));
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "FromInt8");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Flag");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "EnumValue");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::FunctionHasOpcode(*Emitted.Image, asBC_ADDi)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionOptions Options;
		Options.MaxFrameDWords = 256;
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::Succeeded, Emitted.EmissionStatus));
		ASSERT_THAT(IsTrue(VMSourceNumericPrivate::MaxFrameSize(*Emitted.Image) > 64));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Builder.TakeDefinitions())));
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Function = FindSourceFunction(Builder, "Run");
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Emitted.Image, Snapshot)));
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
		asCBuilder Builder(Input.Snapshot, Input.Diagnostics, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(), DescribeBuilderFailure(Builder)));
		asSByteCodeEmissionOptions Options;
		Options.MaxFrameDWords = 16;
		asSByteCodeEmissionResult Emitted = EmitFromSession(Builder, Options);
		ASSERT_THAT(AreEqual(asEByteCodeEmissionStatus::ResourceLimit, Emitted.EmissionStatus));
		ASSERT_THAT(IsNull(Emitted.Image.Get()));
	}
};

#endif
