#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMLifetimeAdmission, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FImageCase
	{
		FDetachedDefinitionFixture Definitions;
		asCByteCodeImageBuilder Builder;
		asCObjectType* Type = nullptr;
		asCScriptFunction* Constructor = nullptr;
		asCScriptFunction* Run = nullptr;
		asUINT TypeSlot = 0;
		asUINT ConstructorSlot = 0;

		bool Start(bool bConstructor = false)
		{
			Type = Definitions.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
			if (!Type) return false;
			if (bConstructor)
				Constructor = Definitions.DefineFunction(TEXT("Item"), TEXT(""), asFUNC_SCRIPT,
					Definitions.Set->Primitive(ttVoid), {}, Type, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Constructor);
			Run = Definitions.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Definitions.Set->Primitive(ttVoid), {});
			if (!Run || (bConstructor && !Constructor) || !Definitions.FinalizeAndFreeze()) return false;
			asUINT RunSlot = 0;
			if (Builder.AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded
				|| Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return false;
			if (Constructor && Builder.AddFunctionRequirement(*Constructor, ConstructorSlot) != asEByteCodeImageStatus::Succeeded) return false;
			if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0) != asEByteCodeImageStatus::Succeeded) return false;
			asSByteCodeFrameDescriptor Frame;
			Frame.LocalDwords = 8;
			Frame.InitializedObjectSlots.Add(2);
			return Builder.SetFrameDescriptor(Frame) == asEByteCodeImageStatus::Succeeded;
		}

		bool Record(asEByteCodeCleanupPath Path)
		{
			asSByteCodeCleanupAction Action;
			Action.Kind = asEByteCodeCleanupKind::DestroyObject;
			Action.Path = Path;
			Action.FrameOffset = 2;
			Action.TypeSlot = TypeSlot;
			Action.InstructionIndex = Builder.GetInstructionCount() - 1;
			return Builder.AddCleanupAction(Action) == asEByteCodeImageStatus::Succeeded;
		}

		bool Allocate(bool bInitialized = true)
		{
			asSByteCodeOperand Local = ByteLocal(2);
			asSByteCodeOperand Alloc[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
			return Builder.Emit(asBC_PSF, MakeArrayView(&Local, 1)) == asEByteCodeImageStatus::Succeeded
				&& Builder.Emit(asBC_ALLOC, MakeArrayView(Alloc, 2)) == asEByteCodeImageStatus::Succeeded
				&& (!bInitialized || Record(asEByteCodeCleanupPath::Exception));
		}

		bool Construct()
		{
			asSByteCodeOperand Receiver = ByteLocal(2);
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, ConstructorSlot);
			return Builder.Emit(asBC_PshVPtr, MakeArrayView(&Receiver, 1)) == asEByteCodeImageStatus::Succeeded
				&& Builder.Emit(asBC_CALL, MakeArrayView(&Call, 1)) == asEByteCodeImageStatus::Succeeded
				&& Record(asEByteCodeCleanupPath::Exception);
		}

		bool Destroy()
		{
			asSByteCodeOperand Free[2] = {ByteLocal(2), ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
			return Builder.Emit(asBC_FREE, MakeArrayView(Free, 2)) == asEByteCodeImageStatus::Succeeded
				&& Record(asEByteCodeCleanupPath::Normal);
		}

		bool Jump(asEBCInstr Opcode, asUINT Label)
		{
			asSByteCodeOperand Target;
			Target.Kind = asEByteCodeOperandKind::Label;
			Target.Label = Label;
			return Builder.Emit(Opcode, MakeArrayView(&Target, 1)) == asEByteCodeImageStatus::Succeeded;
		}

		TUniquePtr<asCByteCodeImage> Finish()
		{
			asSByteCodeOperand Ret = ByteImm16(0);
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded
				|| Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
			TUniquePtr<asCByteCodeImage> Image;
			if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
			return Image;
		}
	};

public:
	TEST_METHOD(LinearConstructionAndDestructionPassDirectAndDecoded)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Decoded)));
	}

	TEST_METHOD(BranchBypassingAllocationBeforeDestroyRejects)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		const asUINT Free = Case.Builder.NewLabel();
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JZ, Free)));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Free)));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidOperand, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(BranchBypassingOnlyNormalCleanupRejects)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		const asUINT Done = Case.Builder.NewLabel();
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JNZ, Done)));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Done)));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(CleanupRecordsMustDescribeActualOperations)
	{
		for (asEByteCodeCleanupPath Path : {asEByteCodeCleanupPath::Normal, asEByteCodeCleanupPath::Exception})
		{
			FImageCase Case;
			ASSERT_THAT(IsTrue(Case.Start()));
			ASSERT_THAT(IsTrue(Case.Allocate()));
			ASSERT_THAT(IsTrue(Case.Destroy()));
			TUniquePtr<asCByteCodeImage> Image = Case.Finish();
			ASSERT_THAT(IsNotNull(Image.Get()));
			for (auto& Action : Image->Functions[0].Cleanup)
				if (Action.Path == Path) Action.InstructionIndex = 3; // RET is neither init nor free.
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
		}
	}

	TEST_METHOD(DestroyRevisitedWithoutReconstructionRejects)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		const asUINT Free = Case.Builder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Free)));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JNZ, Free)));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ReconstructedInstancesMayEachBeDestroyed)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
	}

	TEST_METHOD(ExclusiveBranchesEachConstructAndDestroyPass)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		const asUINT Else = Case.Builder.NewLabel(), Join = Case.Builder.NewLabel();
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JZ, Else)));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JMP, Join)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Else)));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Join)));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ConstructionAndDestructionBalanceLoop)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		const asUINT Start = Case.Builder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Start)));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JNZ, Start)));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(SkippedConstructorBeforeDestroyRejects)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start(true)));
		ASSERT_THAT(IsTrue(Case.Allocate(false)));
		const asUINT Free = Case.Builder.NewLabel();
		ASSERT_THAT(IsTrue(Case.Jump(asBC_JZ, Free)));
		ASSERT_THAT(IsTrue(Case.Construct()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.BindLabel(Free)));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(CompletedConstructorBeforeDestroyPasses)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start(true)));
		ASSERT_THAT(IsTrue(Case.Allocate(false)));
		ASSERT_THAT(IsTrue(Case.Construct()));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(MissingInitializationGuardRejects)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate(false)));
		ASSERT_THAT(IsTrue(Case.Destroy()));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ExceptionExitUsesLiveGuardWithoutNormalDestroy)
	{
		FImageCase Case;
		ASSERT_THAT(IsTrue(Case.Start()));
		ASSERT_THAT(IsTrue(Case.Allocate()));
		asSByteCodeOperand Kind = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_ThrowException, MakeArrayView(&Kind, 1))));
		TUniquePtr<asCByteCodeImage> Image = Case.Finish(); // Unreachable RET does not require normal destruction.
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}
};

#endif
