#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_verifier.h"
#include "frontend/as_stable_key.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMFlowAdmission, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey Key(uint8 Marker)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = 0xAD;
		Bytes[1] = Marker;
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

	static asSByteCodeOperand Imm64(int64 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS64;
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

	static asSByteCodeOperand FunctionSlot(asUINT Slot)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::FunctionSlot;
		Operand.Slot = Slot;
		return Operand;
	}

	static asSByteCodeRequirement TypeUseDwords(const asSStableKey& UseKey, int32 Dwords)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::TypeUse;
		Requirement.Key = UseKey;
		Requirement.NativeOffset = Dwords;
		return Requirement;
	}

	static asSByteCodeRequirement VoidDoubleFunction(const asSStableKey& FunctionKey, const asSStableKey& DoubleUse)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Function;
		Requirement.Key = FunctionKey;
		Requirement.Name = TEXT("TakeDouble");
		asSByteCodeParameterContract Parameter;
		Parameter.TypeUse = DoubleUse;
		Parameter.PassingMode = 0;
		Requirement.Parameters.Add(Parameter);
		Requirement.ReturnTypeUse = Key(0x10);
		return Requirement;
	}

	static asSByteCodeRequirement VoidInRefFunction(const asSStableKey& FunctionKey, const asSStableKey& IntUse)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Function;
		Requirement.Key = FunctionKey;
		Requirement.Name = TEXT("TakeInRef");
		asSByteCodeParameterContract Parameter;
		Parameter.TypeUse = IntUse;
		Parameter.PassingMode = 1;
		Requirement.Parameters.Add(Parameter);
		Requirement.ReturnTypeUse = Key(0x11);
		return Requirement;
	}

	static TUniquePtr<asCByteCodeImage> FinishImage(asCByteCodeImageBuilder& Builder)
	{
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}

public:
	TEST_METHOD(VoidDoubleCallWithOneDwordPushRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey DoubleUse = Key(0x21);
		const asSStableKey Callee = Key(0x22);
		const asSStableKey Run = Key(0x23);
		asUINT UseSlot = 0;
		asUINT CalleeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeUseDwords(DoubleUse, 2), UseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(VoidDoubleFunction(Callee, DoubleUse), CalleeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run, Run, 0, 0)));
		asSByteCodeOperand Push = Imm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Call = FunctionSlot(CalleeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
		ASSERT_THAT(AreEqual(1, Fault.InstructionIndex));
	}

	TEST_METHOD(VoidDoubleCallWithPshC8SucceedsAndBalancedLoopSucceeds)
	{
		asCByteCodeImageBuilder Linear;
		const asSStableKey DoubleUse = Key(0x24);
		const asSStableKey Callee = Key(0x25);
		const asSStableKey Run = Key(0x26);
		asUINT UseSlot = 0;
		asUINT CalleeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.AddRequirement(TypeUseDwords(DoubleUse, 2), UseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.AddRequirement(VoidDoubleFunction(Callee, DoubleUse), CalleeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.BeginFunction(Run, Run, 0, 0)));
		asSByteCodeOperand Wide = Imm64(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.Emit(asBC_PshC8, MakeArrayView(&Wide, 1))));
		asSByteCodeOperand Call = FunctionSlot(CalleeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Linear.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> LinearImage = FinishImage(Linear);
		ASSERT_THAT(IsNotNull(LinearImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*LinearImage)));

		asCByteCodeImageBuilder Loop;
		const asSStableKey LoopFn = Key(0x27);
		asUINT LoopUse = 0;
		asUINT LoopCallee = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.AddRequirement(TypeUseDwords(DoubleUse, 2), LoopUse)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.AddRequirement(VoidDoubleFunction(Callee, DoubleUse), LoopCallee)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.BeginFunction(LoopFn, LoopFn, 0, 0)));
		const asUINT LoopTop = Loop.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.BindLabel(LoopTop)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.Emit(asBC_PshC8, MakeArrayView(&Wide, 1))));
		asSByteCodeOperand LoopCall = FunctionSlot(LoopCallee);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.Emit(asBC_CALLSYS, MakeArrayView(&LoopCall, 1))));
		asSByteCodeOperand Back = LabelAt(LoopTop);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Loop.Emit(asBC_JMP, MakeArrayView(&Back, 1))));
		TUniquePtr<asCByteCodeImage> LoopImage = FinishImage(Loop);
		ASSERT_THAT(IsNotNull(LoopImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*LoopImage)));
	}

	TEST_METHOD(InRefCallWithOneDwordPushRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey IntUse = Key(0x28);
		const asSStableKey Callee = Key(0x29);
		const asSStableKey Run = Key(0x2A);
		asUINT UseSlot = 0;
		asUINT CalleeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeUseDwords(IntUse, 1), UseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(VoidInRefFunction(Callee, IntUse), CalleeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run, Run, 0, 0)));
		asSByteCodeOperand Push = Imm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Call = FunctionSlot(CalleeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
		ASSERT_THAT(AreEqual(1, Fault.InstructionIndex));
	}

	TEST_METHOD(FinalConditionalFallthroughRejectsAndFinalJumpSucceeds)
	{
		asCByteCodeImageBuilder Conditional;
		const asSStableKey CondFn = Key(0x2B);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Conditional.BeginFunction(CondFn, CondFn, 4, 0)));
		const asUINT CondStart = Conditional.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Conditional.BindLabel(CondStart)));
		asSByteCodeOperand Acc = {};
		Acc.Kind = asEByteCodeOperandKind::Local;
		Acc.Immediate = 0;
		asSByteCodeOperand SetOps[2] = {Acc, Imm32(1)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Conditional.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Conditional.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Target = LabelAt(CondStart);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Conditional.Emit(asBC_JZ, MakeArrayView(&Target, 1))));
		TUniquePtr<asCByteCodeImage> CondImage = FinishImage(Conditional);
		ASSERT_THAT(IsNotNull(CondImage.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*CondImage);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, Fault.Status));
		ASSERT_THAT(AreEqual(2, Fault.InstructionIndex));

		asCByteCodeImageBuilder Jump;
		const asSStableKey JumpFn = Key(0x2C);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Jump.BeginFunction(JumpFn, JumpFn, 0, 0)));
		const asUINT JumpStart = Jump.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Jump.BindLabel(JumpStart)));
		asSByteCodeOperand Back = LabelAt(JumpStart);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Jump.Emit(asBC_JMP, MakeArrayView(&Back, 1))));
		TUniquePtr<asCByteCodeImage> JumpImage = FinishImage(Jump);
		ASSERT_THAT(IsNotNull(JumpImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*JumpImage)));
	}

	TEST_METHOD(LiveSlotWithoutNormalCleanupRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = Key(0x2D);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = Key(0x40);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ExclusiveBranchNormalDestroysSucceedAndSamePathDuplicatesReject)
	{
		asCByteCodeImageBuilder Exclusive;
		const asSStableKey Function = Key(0x2E);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = Key(0x41);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction ThenDestroy;
		ThenDestroy.Kind = asEByteCodeCleanupKind::DestroyObject;
		ThenDestroy.Path = asEByteCodeCleanupPath::Normal;
		ThenDestroy.FrameOffset = 0;
		ThenDestroy.TypeSlot = TypeSlot;
		ThenDestroy.InstructionIndex = 1;
		asSByteCodeCleanupAction ElseDestroy = ThenDestroy;
		ElseDestroy.InstructionIndex = 2;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.AddCleanupAction(ThenDestroy)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.AddCleanupAction(ElseDestroy)));
		const asUINT Else = Exclusive.NewLabel();
		const asUINT Done = Exclusive.NewLabel();
		asSByteCodeOperand ElseOp = LabelAt(Else);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.Emit(asBC_JZ, MakeArrayView(&ElseOp, 1))));
		asSByteCodeOperand DoneOp = LabelAt(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.Emit(asBC_JMP, MakeArrayView(&DoneOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.BindLabel(Else)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.Emit(asBC_JMP, MakeArrayView(&DoneOp, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.BindLabel(Done)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Exclusive.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> ExclusiveImage = FinishImage(Exclusive);
		ASSERT_THAT(IsNotNull(ExclusiveImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*ExclusiveImage)));

		asCByteCodeImageBuilder Duplicate;
		const asSStableKey DupFn = Key(0x2F);
		asUINT DupType = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.AddRequirement(TypeReq, DupType)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.BeginFunction(DupFn, DupFn, 4, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction First = ThenDestroy;
		First.InstructionIndex = 0;
		asSByteCodeCleanupAction Second = First;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.AddCleanupAction(First)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.AddCleanupAction(Second)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Duplicate.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> DupImage = FinishImage(Duplicate);
		ASSERT_THAT(IsNotNull(DupImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*DupImage)));
	}

	TEST_METHOD(ResultSpanBeyondFrameAndOverflowReject)
	{
		asCByteCodeImageBuilder Over;
		const asSStableKey OverFn = Key(0x30);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Over.BeginFunction(OverFn, OverFn, 64, 0)));
		asSByteCodeFrameDescriptor OverFrame;
		OverFrame.LocalDwords = 64;
		OverFrame.ResultDwords = 65;
		OverFrame.ResultOffset = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Over.SetFrameDescriptor(OverFrame)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Over.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> OverImage = FinishImage(Over);
		ASSERT_THAT(IsNotNull(OverImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*OverImage)));

		asCByteCodeImageBuilder Fit;
		const asSStableKey FitFn = Key(0x31);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Fit.BeginFunction(FitFn, FitFn, 65, 0)));
		asSByteCodeFrameDescriptor FitFrame;
		FitFrame.LocalDwords = 65;
		FitFrame.ResultDwords = 65;
		FitFrame.ResultOffset = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Fit.SetFrameDescriptor(FitFrame)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Fit.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> FitImage = FinishImage(Fit);
		ASSERT_THAT(IsNotNull(FitImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*FitImage)));

		asCByteCodeImageBuilder Huge;
		const asSStableKey HugeFn = Key(0x32);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Huge.BeginFunction(HugeFn, HugeFn, 4, 0)));
		asSByteCodeFrameDescriptor HugeFrame;
		HugeFrame.LocalDwords = 4;
		HugeFrame.ResultDwords = 0x40000000u;
		HugeFrame.ResultOffset = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Huge.SetFrameDescriptor(HugeFrame)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Huge.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> HugeImage = FinishImage(Huge);
		ASSERT_THAT(IsNotNull(HugeImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*HugeImage)));
	}
};

#endif
