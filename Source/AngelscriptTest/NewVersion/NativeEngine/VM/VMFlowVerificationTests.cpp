#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_verifier.h"
#include "frontend/Basic/as_stable_key.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMFlowVerification, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey FlowKey(uint8 Marker)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = Marker;
		asSStableKey Result;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Result);
		return Result;
	}

	static asSByteCodeOperand Imm16(int16 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS16;
		Operand.Immediate = Value;
		return Operand;
	}

	static asSByteCodeOperand Imm32(int32 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS32;
		Operand.Immediate = Value;
		return Operand;
	}

	static asSByteCodeOperand LabelAt(asUINT Index)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Index;
		Operand.Immediate = static_cast<int64>(Index);
		return Operand;
	}

	static asSByteCodeOperand Local(int32 Offset)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Offset;
		return Operand;
	}

	static TUniquePtr<asCByteCodeImage> FinishRet(asCByteCodeImageBuilder& Builder)
	{
		asSByteCodeOperand Ret = Imm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}

public:
	TEST_METHOD(PopPtrWithoutPushUnderflows)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = FlowKey(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 0, 0)));
		TArray<asSByteCodeOperand> NoOperands;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, NoOperands)));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
	}

	TEST_METHOD(UnequalBranchJoinAndGrowingBackedgeReject)
	{
		asCByteCodeImageBuilder JoinBuilder;
		const asSStableKey JoinKey = FlowKey(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.BeginFunction(JoinKey, JoinKey, 0, 0)));
		const asUINT Else = JoinBuilder.NewLabel();
		const asUINT Done = JoinBuilder.NewLabel();
		asSByteCodeOperand ToElse = LabelAt(Else);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.Emit(asBC_JZ, MakeArrayView(&ToElse, 1))));
		asSByteCodeOperand Push = Imm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand ToDone = LabelAt(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.Emit(asBC_JMP, MakeArrayView(&ToDone, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.BindLabel(Else)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JoinBuilder.BindLabel(Done)));
		TUniquePtr<asCByteCodeImage> JoinImage = FinishRet(JoinBuilder);
		ASSERT_THAT(IsNotNull(JoinImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, asCByteCodeVerifier::Verify(*JoinImage)));

		asCByteCodeImageBuilder LoopBuilder;
		const asSStableKey LoopKey = FlowKey(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, LoopBuilder.BeginFunction(LoopKey, LoopKey, 0, 0)));
		const asUINT Top = LoopBuilder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, LoopBuilder.BindLabel(Top)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, LoopBuilder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Back = LabelAt(Top);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, LoopBuilder.Emit(asBC_JMP, MakeArrayView(&Back, 1))));
		TUniquePtr<asCByteCodeImage> LoopImage = FinishRet(LoopBuilder);
		ASSERT_THAT(IsNotNull(LoopImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, asCByteCodeVerifier::Verify(*LoopImage)));
	}

	TEST_METHOD(ValidBalancedLoopAndGuardedCleanupPass)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = FlowKey(4);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = FlowKey(0x40);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeOperand ObjectLocal;
		ObjectLocal.Kind = asEByteCodeOperandKind::Local;
		ObjectLocal.Immediate = 2;
		asSByteCodeOperand ObjectType;
		ObjectType.Kind = asEByteCodeOperandKind::TypeSlot;
		ObjectType.Slot = TypeSlot;
		asSByteCodeOperand Zero;
		Zero.Kind = asEByteCodeOperandKind::ImmediateS32;
		Zero.Immediate = 0;
		asSByteCodeOperand AllocOps[2] = {ObjectType, Zero};
		asSByteCodeOperand FreeOps[2] = {ObjectLocal, ObjectType};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&ObjectLocal, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		const asUINT Top = Builder.NewLabel();
		const asUINT Done = Builder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Top)));
		asSByteCodeOperand ToDone = LabelAt(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JZ, MakeArrayView(&ToDone, 1))));
		asSByteCodeOperand Back = LabelAt(Top);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&Back, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Done)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction Cleanup;
		Cleanup.Kind = asEByteCodeCleanupKind::DestroyObject;
		Cleanup.Path = asEByteCodeCleanupPath::Exception;
		Cleanup.InstructionIndex = 1;
		Cleanup.FrameOffset = 2;
		Cleanup.TypeSlot = TypeSlot;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		Cleanup.Path = asEByteCodeCleanupPath::Normal;
		Cleanup.InstructionIndex = 4;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(DuplicateDestroyAndUnconstructedCleanupReject)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = FlowKey(5);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = FlowKey(0x41);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeFrameDescriptor Live;
		Live.LocalDwords = 4;
		Live.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Live)));
		asSByteCodeCleanupAction First;
		First.Kind = asEByteCodeCleanupKind::DestroyObject;
		First.FrameOffset = 2;
		First.TypeSlot = TypeSlot;
		asSByteCodeCleanupAction Second = First;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(First)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Second)));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));

		asCByteCodeImageBuilder Unconstructed;
		const asSStableKey Other = FlowKey(6);
		asUINT GhostTypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unconstructed.AddRequirement(TypeReq, GhostTypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unconstructed.BeginFunction(Other, Other, 4, 0)));
		asSByteCodeCleanupAction Ghost;
		Ghost.Kind = asEByteCodeCleanupKind::DestroyObject;
		Ghost.FrameOffset = 2;
		Ghost.Path = asEByteCodeCleanupPath::Exception;
		Ghost.TypeSlot = GhostTypeSlot;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Unconstructed.AddCleanupAction(Ghost)));
		TUniquePtr<asCByteCodeImage> GhostImage = FinishRet(Unconstructed);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*GhostImage)));
	}

	TEST_METHOD(MutationInvalidatesPriorAdmission)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = FlowKey(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 0, 0)));
		asSByteCodeOperand Push = Imm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		ASSERT_THAT(IsTrue(Image->bValidated));
		Image->Functions[0].Instructions[0].Opcode = asBC_PopPtr;
		Image->Functions[0].Instructions[0].Operands.Reset();
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, asCByteCodeVerifier::Verify(*Image)));
		ASSERT_THAT(IsFalse(Image->bValidated));
	}
};

#endif
