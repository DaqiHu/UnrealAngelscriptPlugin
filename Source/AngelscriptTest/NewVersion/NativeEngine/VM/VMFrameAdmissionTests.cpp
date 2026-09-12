#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMFrameAdmission, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFrameCase
	{
		FDetachedDefinitionFixture Definitions;
		asCScriptFunction* Run = nullptr;
		asCObjectType* Type = nullptr;
		asCByteCodeImageBuilder Builder;
		asUINT Parameters = 0;
		asUINT TypeSlot = 0;

		bool Start(asUINT Locals, asUINT Arguments = 0, bool bObjects = false, bool bWideReturn = false)
		{
			Parameters = Arguments;
			TArray<asSMetadataParameter> Params;
			for (asUINT Index = 0; Index < Arguments; ++Index)
			{
				asSMetadataParameter Param;
				Param.Type = Definitions.Set->Primitive(ttInt);
				Params.Add(Param);
			}
			if (bObjects) Type = Definitions.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
			Run = Definitions.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Definitions.Set->Primitive(bWideReturn ? ttInt64 : ttInt), Params);
			if (!Run || !Definitions.FinalizeAndFreeze()) return false;
			asUINT RunSlot = 0;
			if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return false;
			if (Type && Builder.AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return false;
			return Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), Locals, Arguments) == asEByteCodeImageStatus::Succeeded;
		}

		bool Store(int32 Offset, bool bWide = false, int64 Value = 21)
		{
			asSByteCodeOperand Ops[2] = {ByteLocal(Offset), bWide ? ByteImm64(Value) : ByteImm32(static_cast<int32>(Value))};
			return Builder.Emit(bWide ? asBC_SetV8 : asBC_SetV4, MakeArrayView(Ops, 2)) == asEByteCodeImageStatus::Succeeded;
		}

		bool ObjectOp(int32 Offset, bool bInitialize)
		{
			asSByteCodeOperand Local = ByteLocal(Offset);
			if (bInitialize && Builder.Emit(asBC_PSF, MakeArrayView(&Local, 1)) != asEByteCodeImageStatus::Succeeded) return false;
			asSByteCodeOperand Ops[2] = {bInitialize ? ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot) : Local,
				bInitialize ? ByteImm32(0) : ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
			if (Builder.Emit(bInitialize ? asBC_ALLOC : asBC_FREE, MakeArrayView(Ops, 2)) != asEByteCodeImageStatus::Succeeded) return false;
			asSByteCodeCleanupAction Action;
			Action.Kind = asEByteCodeCleanupKind::DestroyObject;
			Action.Path = bInitialize ? asEByteCodeCleanupPath::Exception : asEByteCodeCleanupPath::Normal;
			Action.FrameOffset = Offset;
			Action.TypeSlot = TypeSlot;
			Action.InstructionIndex = Builder.GetInstructionCount() - 1;
			return Builder.AddCleanupAction(Action) == asEByteCodeImageStatus::Succeeded;
		}

		TUniquePtr<asCByteCodeImage> Finish()
		{
			asSByteCodeOperand Ret = ByteImm16(static_cast<int16>(Parameters));
			if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded
				|| Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
			TUniquePtr<asCByteCodeImage> Image;
			if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
			return Image;
		}
	};

public:
	TEST_METHOD(NoArgumentZeroLocalRejectsDirectAndDecoded)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4)));
		ASSERT_THAT(IsTrue(Case.Store(0)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		const auto Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, Fault.Status));
		ASSERT_THAT(AreEqual(0, Fault.InstructionIndex));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidOperand, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(ArgumentPastItsLastDwordRejects)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4, 1)));
		ASSERT_THAT(IsTrue(Case.Store(-1)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(WideLocalCannotCrossIntoArguments)
	{
		for (asUINT Arguments : {0u, 2u})
		{
			FFrameCase Case;
			ASSERT_THAT(IsTrue(Case.Start(4, Arguments)));
			ASSERT_THAT(IsTrue(Case.Store(1, true)));
			auto Image = Case.Finish();
			ASSERT_THAT(IsNotNull(Image.Get()));
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
		}
	}

	TEST_METHOD(WideArgumentCannotCrossItsEnd)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4, 2)));
		ASSERT_THAT(IsTrue(Case.Store(-1, true)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(FirstAndLastCompleteLocalsPassDirectAndDecoded)
	{
		for (bool bWide : {false, true})
			for (int32 Offset : {bWide ? 2 : 1, 4})
			{
				FFrameCase Case;
				ASSERT_THAT(IsTrue(Case.Start(4)));
				ASSERT_THAT(IsTrue(Case.Store(Offset, bWide)));
				auto Image = Case.Finish();
				ASSERT_THAT(IsNotNull(Image.Get()));
				ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
				TArray<uint8> Bytes;
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
				TUniquePtr<asCByteCodeImage> Decoded;
				ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
			}
	}

	TEST_METHOD(FirstAndLastCompleteArgumentsPass)
	{
		for (bool bWide : {false, true})
			for (int32 Offset : {0, bWide ? -2 : -3})
			{
				FFrameCase Case;
				ASSERT_THAT(IsTrue(Case.Start(4, 4)));
				ASSERT_THAT(IsTrue(Case.Store(Offset, bWide)));
				auto Image = Case.Finish();
				ASSERT_THAT(IsNotNull(Image.Get()));
				ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
			}
	}

	TEST_METHOD(SignedPositiveEdgeUsesPhysicalStorageDirection)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(32767)));
		ASSERT_THAT(IsTrue(Case.Store(32767, true)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		Image->Functions[0].Instructions[0].Operands[0].Immediate = 32768;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ManagedStorageCannotOwnArgumentZero)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(8, 2, true)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 8;
		Frame.ParameterDwords = 2;
		Frame.InitializedObjectSlots.Add(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.SetFrameDescriptor(Frame)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(0, true)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(0, false)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ManagedPointerSpansCannotOverlap)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(8, 0, true)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 8;
		Frame.InitializedObjectSlots = {2, 3};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.SetFrameDescriptor(Frame)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(2, true)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(3, true)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(3, false)));
		ASSERT_THAT(IsTrue(Case.ObjectOp(2, false)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ResultSpanMustStayWithinLocalStorage)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4, 0, false, true)));
		asSByteCodeFrameDescriptor Frame;
		Frame.LocalDwords = 4;
		Frame.ResultDwords = 2;
		Frame.ResultOffset = 1;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.SetFrameDescriptor(Frame)));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
		Image->Functions[0].Frame.ResultOffset = 4;
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(PointerLoadRequiresBothDwordsInsideTheFrame)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4)));
		asSByteCodeOperand Local = ByteLocal(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PshVPtr, MakeArrayView(&Local, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_PopPtr, {})));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidFrame, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(ConversionOperandsUseTheirOwnStorageWidths)
	{
		for (asEBCInstr Opcode : {asBC_iTOi64, asBC_i64TOi})
		{
			FFrameCase Case;
			ASSERT_THAT(IsTrue(Case.Start(2)));
			asSByteCodeOperand Ops[2] = {ByteLocal(Opcode == asBC_iTOi64 ? 2 : 1), ByteLocal(Opcode == asBC_iTOi64 ? 1 : 2)};
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(Opcode, MakeArrayView(Ops, 2))));
			auto Image = Case.Finish();
			ASSERT_THAT(IsNotNull(Image.Get()));
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		}
	}

	TEST_METHOD(SafeLocalBoundaryExecutionReturnsFortyTwo)
	{
		FFrameCase Case;
		ASSERT_THAT(IsTrue(Case.Start(4)));
		ASSERT_THAT(IsTrue(Case.Store(2, true, 21)));
		ASSERT_THAT(IsTrue(Case.Store(4, false, 21)));
		asSByteCodeOperand Add[3] = {ByteLocal(3), ByteLocal(2), ByteLocal(4)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_ADDi, MakeArrayView(Add, 3))));
		asSByteCodeOperand Result = ByteLocal(3);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Case.Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Result, 1))));
		auto Image = Case.Finish();
		ASSERT_THAT(IsNotNull(Image.Get()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(Case.Definitions.TakeSet())));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Case.Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
