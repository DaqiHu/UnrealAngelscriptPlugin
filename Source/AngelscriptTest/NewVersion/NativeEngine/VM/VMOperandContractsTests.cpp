#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_opcode.h"
#include "as_bytecode_verifier.h"
#include "frontend/as_stable_key.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMOperandContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey OperandKey(uint8 Marker)
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

	static asSByteCodeOperand Imm64(int64 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS64;
		Operand.Immediate = Value;
		return Operand;
	}

	static asSByteCodeOperand Local(int32 Offset)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Local;
		Operand.Immediate = Offset;
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

	static asSByteCodeOperand Slot(asEByteCodeOperandKind Kind, asUINT Index)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = Kind;
		Operand.Slot = Index;
		return Operand;
	}

	static TUniquePtr<asCByteCodeImage> FinishRet(asCByteCodeImageBuilder& Builder, int16 Pop = 0)
	{
		asSByteCodeOperand Ret = Imm16(Pop);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}

	static TUniquePtr<asCByteCodeImage> MakeSetV4(asUINT FrameSize, asUINT ParameterSize, int32 Offset, int32 Value)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = OperandKey(1);
		if (Builder.BeginFunction(Function, Function, FrameSize, ParameterSize) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand SetOps[2] = { Local(Offset), Imm32(Value) };
		if (Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return FinishRet(Builder, static_cast<int16>(ParameterSize));
	}

	static asEByteCodeImageStatus DecodeMutated(const asCByteCodeImage& Image, TUniquePtr<asCByteCodeImage>& OutDecoded)
	{
		TArray<uint8> Bytes;
		if (asCByteCodeImageCodec::Encode(Image, Bytes) != asEByteCodeImageStatus::Succeeded)
		{
			return asEByteCodeImageStatus::InvalidArgument;
		}
		return asCByteCodeImageCodec::Decode(Bytes, OutDecoded);
	}

public:
	TEST_METHOD(AssignedOpcodesHaveDescriptorsOrStrRejects)
	{
		int32 Described = 0;
		for (int32 Value = 0; Value <= asBC_MAXBYTECODE; ++Value)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(Value);
			asSByteCodeOpcodeInfo Info;
			if (Opcode == asBC_STR)
			{
				ASSERT_THAT(IsFalse(asByteCodeOpcodeIsExecutable(Opcode)));
				ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(Opcode, Info)));
				continue;
			}
			ASSERT_THAT(IsTrue(asByteCodeOpcodeGetInfo(Opcode, Info)));
			ASSERT_THAT(IsTrue(asByteCodeOpcodeIsExecutable(Opcode)));
			ASSERT_THAT(AreEqual(Opcode, Info.Opcode));
			ASSERT_THAT(IsTrue(Info.Control != asEByteCodeControlKind::None));
			ASSERT_THAT(IsTrue(Info.OperandCount <= 3u));
			for (asUINT Index = 0; Index < Info.OperandCount; ++Index)
			{
				ASSERT_THAT(AreEqual(Info.Operands[Index], Info.OperandInfo[Index].Kind));
			}
			++Described;
		}
		ASSERT_THAT(AreEqual(212, Described));

		asSByteCodeOpcodeInfo JumpInfo;
		ASSERT_THAT(IsTrue(asByteCodeOpcodeGetInfo(asBC_JMP, JumpInfo)));
		ASSERT_THAT(AreEqual(asEByteCodeControlKind::Jump, JumpInfo.Control));
		ASSERT_THAT(AreEqual(asEByteCodeOperandKind::Label, JumpInfo.Operands[0]));
		ASSERT_THAT(AreEqual(static_cast<int8>(4), JumpInfo.OperandInfo[0].SignedWidth));

		asSByteCodeOpcodeInfo SetInfo;
		ASSERT_THAT(IsTrue(asByteCodeOpcodeGetInfo(asBC_SetV4, SetInfo)));
		ASSERT_THAT(AreEqual(asEByteCodeOperandKind::Local, SetInfo.Operands[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(4), SetInfo.OperandInfo[0].StorageWidth));
		ASSERT_THAT(AreEqual(static_cast<uint8>(4), SetInfo.OperandInfo[0].WriteWidth));
		ASSERT_THAT(AreEqual(asEByteCodeOperandKind::ImmediateS32, SetInfo.Operands[1]));
		ASSERT_THAT(AreEqual(static_cast<int8>(4), SetInfo.OperandInfo[1].SignedWidth));
	}

	TEST_METHOD(ReservedAndPseudoOpcodesReject)
	{
		asSByteCodeOpcodeInfo Info;
		for (int32 Value = 213; Value <= 250; ++Value)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(Value);
			ASSERT_THAT(IsFalse(asByteCodeOpcodeIsExecutable(Opcode)));
			ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(Opcode, Info)));
		}
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_VarDecl, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_Block, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_ObjInfo, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_LINE, Info)));
		ASSERT_THAT(IsFalse(asByteCodeOpcodeGetInfo(asBC_LABEL, Info)));
	}

	TEST_METHOD(JmpImmediateS32RejectsWrongTagDirectAndDecoded)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = OperandKey(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 0, 0)));
		const asUINT Done = Builder.NewLabel();
		asSByteCodeOperand Jump = LabelAt(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&Jump, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Done)));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		Image->Functions[0].Instructions[0].Operands[0] = Imm32(99);
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, Fault.Status));
		ASSERT_THAT(IsTrue(Function == Fault.FunctionKey));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
		ASSERT_THAT(AreEqual(0, Fault.OperandIndex));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreNotEqual(asEByteCodeImageStatus::Succeeded, DecodeMutated(*Image, Decoded)));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(SetV4MinLocalRejectsFrameRangeDirectAndDecoded)
	{
		TUniquePtr<asCByteCodeImage> Image = MakeSetV4(4, 0, -32768, 7);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, Fault.Status));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
		ASSERT_THAT(AreEqual(0, Fault.OperandIndex));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreNotEqual(asEByteCodeImageStatus::Succeeded, DecodeMutated(*Image, Decoded)));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(ImmediateWidthSlotRoleAndStringRejects)
	{
		TUniquePtr<asCByteCodeImage> Wide = MakeSetV4(4, 0, 1, 7);
		ASSERT_THAT(IsNotNull(Wide.Get()));
		Wide->Functions[0].Instructions[1].Operands[0].Immediate = 40000;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Wide)));

		TUniquePtr<asCByteCodeImage> CallImage = MakeSetV4(4, 0, 1, 7);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = OperandKey(9);
		CallImage->Requirements.Add(TypeReq);
		CallImage->Functions[0].Instructions[0].Opcode = asBC_CALL;
		CallImage->Functions[0].Instructions[0].Operands = { Slot(asEByteCodeOperandKind::TypeSlot, 0) };
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*CallImage)));

		TUniquePtr<asCByteCodeImage> GlobalImage = MakeSetV4(4, 0, 1, 7);
		GlobalImage->Functions[0].Instructions[0].Opcode = asBC_PGA;
		GlobalImage->Functions[0].Instructions[0].Operands = { Slot(asEByteCodeOperandKind::FunctionSlot, 0) };
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*GlobalImage)));

		TUniquePtr<asCByteCodeImage> PropertyImage = MakeSetV4(4, 0, 1, 7);
		PropertyImage->Functions[0].Instructions[0].Opcode = asBC_ALLOC;
		PropertyImage->Functions[0].Instructions[0].Operands.Reset();
		PropertyImage->Functions[0].Instructions[0].Operands.Add(Slot(asEByteCodeOperandKind::PropertySlot, 0));
		PropertyImage->Functions[0].Instructions[0].Operands.Add(Imm32(0));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*PropertyImage)));

		TUniquePtr<asCByteCodeImage> StringImage = MakeSetV4(4, 0, 1, 7);
		StringImage->Functions[0].Instructions[0].Operands[1] = Slot(asEByteCodeOperandKind::StringSlot, 99);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*StringImage)));
	}

	TEST_METHOD(FrameSpanAlignmentCrossingAndZeroSizeRejects)
	{
		TUniquePtr<asCByteCodeImage> PastEnd = MakeSetV4(4, 0, 5, 7);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*PastEnd)));

		TUniquePtr<asCByteCodeImage> Zero = MakeSetV4(0, 0, 0, 7);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Zero)));

		TUniquePtr<asCByteCodeImage> MaxOffset = MakeSetV4(4, 0, 32767, 7);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*MaxOffset)));

		asCByteCodeImageBuilder CrossBuilder;
		const asSStableKey CrossKey = OperandKey(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, CrossBuilder.BeginFunction(CrossKey, CrossKey, 4, 0)));
		asSByteCodeOperand Set8[2] = { Local(1), Imm64(7) };
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, CrossBuilder.Emit(asBC_SetV8, MakeArrayView(Set8, 2))));
		TUniquePtr<asCByteCodeImage> Crossing = FinishRet(CrossBuilder);
		ASSERT_THAT(IsNotNull(Crossing.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Crossing)));

		asCByteCodeImageBuilder OverflowBuilder;
		const asSStableKey OverflowKey = OperandKey(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, OverflowBuilder.BeginFunction(OverflowKey, OverflowKey, 40000, 40000)));
		asSByteCodeOperand OverflowOps[2] = { Local(-32768), Imm64(1) };
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, OverflowBuilder.Emit(asBC_SetV8, MakeArrayView(OverflowOps, 2))));
		TUniquePtr<asCByteCodeImage> Overflow = FinishRet(OverflowBuilder);
		ASSERT_THAT(IsNotNull(Overflow.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Overflow)));
	}

	TEST_METHOD(ValidNegativeParameterAndInRangeLocalPass)
	{
		TUniquePtr<asCByteCodeImage> LocalOk = MakeSetV4(4, 0, 1, 7);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*LocalOk)));
		ASSERT_THAT(AreEqual(4u, LocalOk->Functions[0].FrameSize));

		TUniquePtr<asCByteCodeImage> ArgOk = MakeSetV4(4, 2, -1, 7);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*ArgOk)));

		asCByteCodeImageBuilder JumpBuilder;
		const asSStableKey JumpKey = OperandKey(5);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JumpBuilder.BeginFunction(JumpKey, JumpKey, 0, 0)));
		const asUINT Done = JumpBuilder.NewLabel();
		asSByteCodeOperand Jump = LabelAt(Done);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JumpBuilder.Emit(asBC_JMP, MakeArrayView(&Jump, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, JumpBuilder.BindLabel(Done)));
		TUniquePtr<asCByteCodeImage> JumpOk = FinishRet(JumpBuilder);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*JumpOk)));
	}

	TEST_METHOD(FrameCleanupSourceCapturedAndInvalidResultRegionRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = OperandKey(6);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 2)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.ParameterDwords = 2;
		Frame.ResultDwords = 1;
		Frame.ResultOffset = 1;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = OperandKey(9);
		TypeReq.Name = TEXT("T");
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
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
		asSByteCodeCleanupAction Cleanup;
		Cleanup.Kind = asEByteCodeCleanupKind::DestroyObject;
		Cleanup.Path = asEByteCodeCleanupPath::Exception;
		Cleanup.InstructionIndex = 1;
		Cleanup.FrameOffset = 2;
		Cleanup.TypeSlot = TypeSlot;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		Cleanup.Path = asEByteCodeCleanupPath::Normal;
		Cleanup.InstructionIndex = 2;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Cleanup)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeSourceRecord Source;
		Source.InstructionIndex = 0;
		Source.Line = 12;
		Source.Column = 4;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddSourceRecord(Source)));
		asSByteCodeOperand SetOps[2] = { Local(1), Imm32(7) };
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		TUniquePtr<asCByteCodeImage> Image = FinishRet(Builder, 2);
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(4u, Image->Functions[0].Frame.LocalDwords));
		ASSERT_THAT(AreEqual(2u, Image->Functions[0].Frame.ParameterDwords));
		ASSERT_THAT(AreEqual(1u, Image->Functions[0].Frame.ResultDwords));
		ASSERT_THAT(AreEqual(2, Image->Functions[0].Cleanup.Num()));
		ASSERT_THAT(AreEqual(1, Image->Functions[0].Source.Num()));
		ASSERT_THAT(AreEqual(12, Image->Functions[0].Source[0].Line));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));

		Image->Functions[0].Frame.ResultDwords = 2;
		Image->Functions[0].Frame.ResultOffset = 1;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}
};

#endif
