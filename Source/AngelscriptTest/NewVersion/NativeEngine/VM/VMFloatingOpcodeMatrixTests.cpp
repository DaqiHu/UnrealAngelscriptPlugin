#include "CQTest.h"
#include "NativeVMTestSupport.h"

#include <limits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace VMFloatingOpcodeMatrixPrivate
{
	asSByteCodeOperand Loc(int32 Offset)
	{
		return ByteLocal(Offset);
	}

	asSByteCodeOperand Imm32(int32 Value)
	{
		return ByteImm32(Value);
	}

	asSByteCodeOperand Imm64(int64 Value)
	{
		return ByteImm64(Value);
	}

	asSByteCodeOperand Imm16(int16 Value)
	{
		return ByteImm16(Value);
	}

	int32 BitsF(float Value)
	{
		int32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	int64 BitsD(double Value)
	{
		int64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	asEByteCodeImageStatus Set4(asCByteCodeImageBuilder& Builder, int32 Offset, int32 Value)
	{
		asSByteCodeOperand Ops[2] = {Loc(Offset), Imm32(Value)};
		return Builder.Emit(asBC_SetV4, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Set8(asCByteCodeImageBuilder& Builder, int32 Offset, int64 Value)
	{
		asSByteCodeOperand Ops[2] = {Loc(Offset), Imm64(Value)};
		return Builder.Emit(asBC_SetV8, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus SetF(asCByteCodeImageBuilder& Builder, int32 Offset, float Value)
	{
		return Set4(Builder, Offset, BitsF(Value));
	}

	asEByteCodeImageStatus SetD(asCByteCodeImageBuilder& Builder, int32 Offset, double Value)
	{
		return Set8(Builder, Offset, BitsD(Value));
	}

	asEByteCodeImageStatus Ternary(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Dest, int32 Left, int32 Right)
	{
		asSByteCodeOperand Ops[3] = {Loc(Dest), Loc(Left), Loc(Right)};
		return Builder.Emit(Opcode, MakeArrayView(Ops, 3));
	}

	asEByteCodeImageStatus Unary(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Offset)
	{
		asSByteCodeOperand Op = Loc(Offset);
		return Builder.Emit(Opcode, MakeArrayView(&Op, 1));
	}

	asEByteCodeImageStatus Pair(asCByteCodeImageBuilder& Builder, asEBCInstr Opcode, int32 Dest, int32 Source)
	{
		asSByteCodeOperand Ops[2] = {Loc(Dest), Loc(Source)};
		return Builder.Emit(Opcode, MakeArrayView(Ops, 2));
	}

	asEByteCodeImageStatus Ret4(asCByteCodeImageBuilder& Builder, int32 Offset)
	{
		asSByteCodeOperand Copy = Loc(Offset);
		const asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = Imm16(0);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	asEByteCodeImageStatus Ret8(asCByteCodeImageBuilder& Builder, int32 Offset)
	{
		asSByteCodeOperand Copy = Loc(Offset);
		const asEByteCodeImageStatus Status = Builder.Emit(asBC_CpyVtoR8, MakeArrayView(&Copy, 1));
		if (Status != asEByteCodeImageStatus::Succeeded) return Status;
		asSByteCodeOperand Ret = Imm16(0);
		return Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
	}

	struct FFloatCase
	{
		FDetachedDefinitionFixture Fixture;
		FNativeEngineOwner Engine;
		asCScriptFunction* Function = nullptr;
		asCByteCodeImageBuilder Builder;
		TUniquePtr<asCByteCodeImage> Image;
		asCExecutableSnapshot* Snapshot = nullptr;
		asIScriptContext* Context = nullptr;

		bool Start(const TCHAR* Name, eTokenType Return, asUINT FrameSize)
		{
			Function = Fixture.DefineFunction(Name, TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(Return), {});
			if (!Function || !Fixture.FinalizeAndFreeze()) return false;
			if (RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set)) != asEMetadataRegistrationResult::Succeeded) return false;
			asUINT Slot = 0;
			if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded) return false;
			return Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), FrameSize, 0)
				== asEByteCodeImageStatus::Succeeded;
		}

		bool Close()
		{
			if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return false;
			if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return false;
			asCExecutableSnapshot* Linked = nullptr;
			if (asLinkByteCodeImage(*Engine.Ptr, *Image, Linked) != asEByteCodeLinkStatus::Succeeded) return false;
			Snapshot = Linked;
			Context = Engine.Ptr->CreateContext();
			return Context && Context->Prepare(Function) == 0;
		}

		int Execute()
		{
			return Context->Execute();
		}

		float RetF() const
		{
			return Context->GetReturnFloat();
		}

		double RetD() const
		{
			return Context->GetReturnDouble();
		}

		int32 RetI() const
		{
			return static_cast<int32>(Context->GetReturnDWord());
		}

		int64 RetQ() const
		{
			return static_cast<int64>(Context->GetReturnQWord());
		}

		~FFloatCase()
		{
			if (Context) Context->Release();
			if (Snapshot) Snapshot->Release();
		}
	};
}

TEST_CLASS_WITH_FLAGS(VMFloatingOpcodeMatrix, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FloatArithmeticAndImmediate)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ADDf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 1.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_ADDf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 3.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SUBf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 8.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_SUBf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 6.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MULf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 2.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 4.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MULf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 10.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 9.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MODf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 8.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MODf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 0.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ADDIf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 1.5f)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(BitsF(2.0f))};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_ADDIf, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 3.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SUBIf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 8.5f)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(BitsF(2.0f))};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_SUBIf, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 6.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MULIf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 2.5f)));
			asSByteCodeOperand Ops[3] = {Loc(2), Loc(1), Imm32(BitsF(4.0f))};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_MULIf, MakeArrayView(Ops, 3))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 10.0f));
		}
	}

	TEST_METHOD(DoubleArithmetic)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("ADDd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 8.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_ADDd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 10.5));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("SUBd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 8.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_SUBd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 6.5));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MULd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 2.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 4.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MULd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 10.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 9.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 4.5));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("MODd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 8.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_MODd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 0.5));
		}
	}

	TEST_METHOD(NegIncDecFloatDouble)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("NEGf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, -4.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_NEGf, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("NEGd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, -6.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_NEGd, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 6.5));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("INCf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 3.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_INCf, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DECf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 5.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_DECf, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("INCd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 3.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_INCd, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 4.5));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DECd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 5.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_LDV, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_DECd, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 4.5));
		}
	}

	TEST_METHOD(FloatDoubleComparisons)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPf"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 1.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CMPf, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TS, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPd"), ttInt, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 8.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CMPd, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TP, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPIf"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 2.0f)));
			asSByteCodeOperand Ops[2] = {Loc(1), Imm32(BitsF(2.0f))};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CMPIf, MakeArrayView(Ops, 2))));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_TZ, TConstArrayView<asSByteCodeOperand>())));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("CMPfNan"), ttInt, 16)));
			const float Nan = std::numeric_limits<float>::quiet_NaN();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, Nan)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 1.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_CMPf, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unary(Case.Builder, asBC_CpyRtoV4, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(1, Case.RetI()));
		}
	}

	TEST_METHOD(IntegerFloatConversions)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("iTOf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_iTOf, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 2.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("fTOi"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 3.75f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_fTOi, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(3, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("uTOf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_uTOf, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("fTOu"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 5.9f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_fTOu, 2, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 2)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(5, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("dTOi"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 3.9)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_dTOi, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(3, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("dTOu"), ttInt, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 7.2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_dTOu, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(AreEqual(7, Case.RetI()));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("dTOf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 4.5)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_dTOf, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.5f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("iTOd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_iTOd, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 2.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("uTOd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 1, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_uTOd, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 4.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("fTOd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 0.5f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_fTOd, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 0.5));
		}
	}

	TEST_METHOD(WideFloatConversions)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("fTOi64"), ttInt64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 8.75f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_fTOi64, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 8));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("dTOi64"), ttInt64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 9.25)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_dTOi64, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 9));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("fTOu64"), ttInt64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 6.2f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_fTOu64, 4, 1)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 6));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("dTOu64"), ttInt64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 11.8)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_dTOu64, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetQ() == 11));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("i64TOf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_i64TOf, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 2.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("u64TOf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_u64TOf, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 1)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 4.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("i64TOd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 8)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_i64TOd, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 8.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("u64TOd"), ttFloat64, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set8(Case.Builder, 2, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Pair(Case.Builder, asBC_u64TOd, 4, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 4)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 16.0));
		}
	}

	TEST_METHOD(PowerFloatAndDomain)
	{
		using namespace VMFloatingOpcodeMatrixPrivate;
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWf"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 2.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 3.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetF() == 8.0f));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWd"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 4.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 16.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("POWdi"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 2.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Set4(Case.Builder, 6, 3)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_POWdi, 8, 2, 6)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 8)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Case.Execute()));
			ASSERT_THAT(IsTrue(Case.RetD() == 8.0));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVfZero"), ttFloat32, 16)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 1, 1.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetF(Case.Builder, 2, 0.0f)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVf, 3, 1, 2)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret4(Case.Builder, 3)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Divide by zero") != nullptr));
		}
		{
			FFloatCase Case;
			ASSERT_THAT(IsTrue(Case.Start(TEXT("DIVdZero"), ttFloat64, 24)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 2, 1.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, SetD(Case.Builder, 4, 0.0)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ternary(Case.Builder, asBC_DIVd, 6, 2, 4)));
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Ret8(Case.Builder, 6)));
			ASSERT_THAT(IsTrue(Case.Close()));
			ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Case.Execute()));
			ASSERT_THAT(IsTrue(FCStringAnsi::Strstr(Case.Context->GetExceptionString(), "Divide by zero") != nullptr));
		}
	}
};

#endif
