#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_verifier.h"
#include "frontend/Basic/as_stable_key.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMByteCodeVerifier, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey Key(uint8 Marker)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = Marker;
		asSStableKey Result;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Result);
		return Result;
	}
	static TUniquePtr<asCByteCodeImage> ConstantReturn()
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = Key(1);
		Builder.BeginFunction(Function, Function, 0, 0);
		asSByteCodeOperand Push;
		Push.Kind = asEByteCodeOperandKind::ImmediateS32;
		Push.Immediate = 7;
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret;
		Ret.Kind = asEByteCodeOperandKind::ImmediateS16;
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		return Image;
	}

public:
	TEST_METHOD(ValidConstantAndLoopPass)
	{
		TUniquePtr<asCByteCodeImage> Image = ConstantReturn();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = Key(2);
		Builder.BeginFunction(Function, Function, 0, 0);
		const asUINT Done = Builder.NewLabel();
		asSByteCodeOperand Jump;
		Jump.Kind = asEByteCodeOperandKind::Label;
		Jump.Label = Done;
		Builder.Emit(asBC_JMP, MakeArrayView(&Jump, 1));
		Builder.BindLabel(Done);
		asSByteCodeOperand Ret;
		Ret.Kind = asEByteCodeOperandKind::ImmediateS16;
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Loop;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Loop)));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Loop)));
	}

	TEST_METHOD(RejectsRetiredReservedAndMissingClosure)
	{
		TUniquePtr<asCByteCodeImage> Image = ConstantReturn();
		Image->Functions[0].Instructions[0].Opcode = asBC_STR;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::UnknownOpcode, asCByteCodeVerifier::Verify(*Image)));
		Image = ConstantReturn();
		Image->Functions[0].Instructions[0].Opcode = static_cast<asEBCInstr>(213);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::UnknownOpcode, asCByteCodeVerifier::Verify(*Image)));
		Image = ConstantReturn();
		Image->Functions[0].Instructions[0].Opcode = asBC_LABEL;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::UnknownOpcode, asCByteCodeVerifier::Verify(*Image)));
		Image = ConstantReturn();
		asSByteCodeRequirement Root;
		Root.Role = asEByteCodeRequirementRole::Type;
		Root.Key = Key(9);
		asSByteCodeRequirement Missing;
		Missing.Key = Key(10);
		Root.ReferencedKeys.Add(Missing.Key);
		Image->Requirements.Add(Root);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::CorruptClosure, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(RejectsOutOfRangeJump)
	{
		TUniquePtr<asCByteCodeImage> Image = ConstantReturn();
		asSByteCodeOperand Jump;
		Jump.Kind = asEByteCodeOperandKind::Label;
		Jump.Immediate = 99;
		asSByteCodeInstruction Instruction;
		Instruction.Opcode = asBC_JMP;
		Instruction.Operands.Add(Jump);
		Image->Functions[0].Instructions.Insert(MoveTemp(Instruction), 0);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, asCByteCodeVerifier::Verify(*Image)));
	}
};

#endif
