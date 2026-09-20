#include "CQTest.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_verifier.h"
#include "frontend/Basic/as_stable_key.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMFlowPath, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSStableKey PathKey(uint8 Marker)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = 0xF0;
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

	static asSByteCodeOperand FunctionSlot(asUINT Slot)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::FunctionSlot;
		Operand.Slot = Slot;
		return Operand;
	}

	static asSByteCodeRequirement MakeTwoIntAddRequirement(const asSStableKey& Key)
	{
		asSByteCodeRequirement Requirement;
		Requirement.Role = asEByteCodeRequirementRole::Function;
		Requirement.Key = Key;
		Requirement.Name = TEXT("Add");
		asSByteCodeParameterContract A;
		A.TypeUse = PathKey(0xA1);
		A.StorageDwords = 1;
		A.StorageKind = asEByteCodeStorageKind::Value;
		asSByteCodeParameterContract B;
		B.TypeUse = PathKey(0xA2);
		B.StorageDwords = 1;
		B.StorageKind = asEByteCodeStorageKind::Value;
		Requirement.Parameters.Add(A);
		Requirement.Parameters.Add(B);
		Requirement.ReturnTypeUse = PathKey(0xA3);
		Requirement.ReturnDwords = 1;
		Requirement.ReturnStorageKind = asEByteCodeStorageKind::Value;
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
	TEST_METHOD(PshC4WithoutReturnRejectsFallthrough)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = PathKey(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 0, 0)));
		asSByteCodeOperand Push = Imm32(42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, Fault.Status));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
	}

	TEST_METHOD(CallSysWithoutArgumentPushesRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey AddKey = PathKey(2);
		const asSStableKey RunKey = PathKey(3);
		asUINT AddSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(MakeTwoIntAddRequirement(AddKey), AddSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunKey, RunKey, 0, 0)));
		asSByteCodeOperand Call = FunctionSlot(AddSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
	}

	TEST_METHOD(CallSysWithTwoPushesThenReturnSucceeds)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey AddKey = PathKey(4);
		const asSStableKey RunKey = PathKey(5);
		asUINT AddSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(MakeTwoIntAddRequirement(AddKey), AddSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(RunKey, RunKey, 0, 0)));
		asSByteCodeOperand Right = Imm32(22);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1))));
		asSByteCodeOperand Left = Imm32(20);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1))));
		asSByteCodeOperand Call = FunctionSlot(AddSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(DisjointNormalAndExceptionCleanupSucceeds)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = PathKey(6);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = PathKey(0x40);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
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
		asSByteCodeCleanupAction Normal;
		Normal.Kind = asEByteCodeCleanupKind::DestroyObject;
		Normal.Path = asEByteCodeCleanupPath::Normal;
		Normal.FrameOffset = 2;
		Normal.TypeSlot = TypeSlot;
		Normal.InstructionIndex = 2;
		asSByteCodeCleanupAction Exception = Normal;
		Exception.Path = asEByteCodeCleanupPath::Exception;
		Exception.InstructionIndex = 1;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Normal)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Exception)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(CleanupPastBodyAndMissingTypeReject)
	{
		asCByteCodeImageBuilder PastBuilder;
		const asSStableKey PastKey = PathKey(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, PastBuilder.BeginFunction(PastKey, PastKey, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, PastBuilder.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction Past;
		Past.Kind = asEByteCodeCleanupKind::DestroyObject;
		Past.Path = asEByteCodeCleanupPath::Both;
		Past.FrameOffset = 2;
		Past.InstructionIndex = 99;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, PastBuilder.AddCleanupAction(Past)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, PastBuilder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> PastImage = FinishImage(PastBuilder);
		ASSERT_THAT(IsNotNull(PastImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*PastImage)));

		asCByteCodeImageBuilder TypeBuilder;
		const asSStableKey TypeKey = PathKey(8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, TypeBuilder.BeginFunction(TypeKey, TypeKey, 4, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, TypeBuilder.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction MissingType;
		MissingType.Kind = asEByteCodeCleanupKind::DestroyObject;
		MissingType.Path = asEByteCodeCleanupPath::Both;
		MissingType.FrameOffset = 2;
		MissingType.TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, TypeBuilder.AddCleanupAction(MissingType)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, TypeBuilder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> TypeImage = FinishImage(TypeBuilder);
		ASSERT_THAT(IsNotNull(TypeImage.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::MissingRequirement, asCByteCodeVerifier::Verify(*TypeImage)));
	}

	TEST_METHOD(BothPathDuplicateDestroyStillRejects)
	{
		asCByteCodeImageBuilder Builder;
		const asSStableKey Function = PathKey(9);
		asSByteCodeRequirement TypeReq;
		TypeReq.Role = asEByteCodeRequirementRole::Type;
		TypeReq.Key = PathKey(0x41);
		asUINT TypeSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(TypeReq, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function, Function, 4, 0)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.InitializedObjectSlots.Add(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.SetFrameDescriptor(Frame)));
		asSByteCodeCleanupAction First;
		First.Kind = asEByteCodeCleanupKind::DestroyObject;
		First.Path = asEByteCodeCleanupPath::Both;
		First.FrameOffset = 2;
		First.TypeSlot = TypeSlot;
		asSByteCodeCleanupAction Second = First;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(First)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddCleanupAction(Second)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		TUniquePtr<asCByteCodeImage> Image = FinishImage(Builder);
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}
};

#endif
