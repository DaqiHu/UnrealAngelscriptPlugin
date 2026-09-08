#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_verifier.h"
#include "as_generic.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMCallAdmission, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asCScriptFunction* DefineCallee(FDetachedDefinitionFixture& Fixture, eTokenType Token, asDWORD Mode = asTM_NONE)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = "value";
		Parameter.Type = Fixture.Image->Primitive(Token);
		Parameter.Flags = Mode;
		if (Mode != asTM_NONE) Parameter.Type.MakeReference(true);
		return Fixture.DefineFunction(TEXT("Take"), TEXT(""), asFUNC_SYSTEM,
			Fixture.Image->Primitive(ttVoid), MakeArrayView(&Parameter, 1));
	}

	static TUniquePtr<asCByteCodeImage> CallImage(asCScriptFunction& Callee, asCScriptFunction& Run,
		asEBCInstr PushOpcode, asSByteCodeOperand Push, bool bLoop = false)
	{
		asCByteCodeImageBuilder Builder;
		asUINT CalleeSlot = 0, RunSlot = 0;
		if (Builder.AddFunctionRequirement(Callee, CalleeSlot) != asEByteCodeImageStatus::Succeeded
			|| Builder.AddFunctionRequirement(Run, RunSlot) != asEByteCodeImageStatus::Succeeded
			|| Builder.BeginFunction(Run.GetStableKey(), Run.GetStableKey(), 8, 0) != asEByteCodeImageStatus::Succeeded)
			return nullptr;
		const asUINT Start = Builder.NewLabel();
		if (Builder.BindLabel(Start) != asEByteCodeImageStatus::Succeeded
			|| Builder.Emit(PushOpcode, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, CalleeSlot);
		if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand End = ByteImm16(0);
		if (bLoop)
		{
			End.Kind = asEByteCodeOperandKind::Label;
			End.Label = Start;
			End.Immediate = Start;
		}
		if (Builder.Emit(bLoop ? asBC_JMP : asBC_RET, MakeArrayView(&End, 1)) != asEByteCodeImageStatus::Succeeded
			|| Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}

	static asSByteCodeRequirement* FindRequirement(asCByteCodeImage& Image, const asSStableKey& Key)
	{
		for (auto& Requirement : Image.Requirements) if (Requirement.Key == Key) return &Requirement;
		return nullptr;
	}

	static void TwiceDouble(asIScriptGeneric* Generic)
	{
		Generic->SetReturnDouble(2.0 * Generic->GetArgDouble(0));
	}

public:
	TEST_METHOD(RealDoubleAndInt64RejectOneDwordArguments)
	{
		for (eTokenType Token : {ttDouble, ttInt64})
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Callee = DefineCallee(Fixture, Token);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
			ASSERT_THAT(IsNotNull(Callee));
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC4, ByteImm32(1));
			ASSERT_THAT(IsNotNull(Image.Get()));
			const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
			ASSERT_THAT(AreEqual(1, Fault.InstructionIndex));
		}
	}

	TEST_METHOD(RealWideCallsBalanceLoopAndDecode)
	{
		for (eTokenType Token : {ttDouble, ttInt64})
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Callee = DefineCallee(Fixture, Token);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(0), true);
			ASSERT_THAT(IsNotNull(Image.Get()));
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
			TUniquePtr<asCByteCodeImage> Decoded;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Decoded)));
		}
	}

	TEST_METHOD(ReferenceModesRejectScalarBitsOfPointerWidth)
	{
		for (asDWORD Mode : {asDWORD(asTM_INREF), asDWORD(asTM_OUTREF), asDWORD(asTM_INOUTREF)})
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Callee = DefineCallee(Fixture, ttInt, Mode);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(123));
			ASSERT_THAT(IsNotNull(Image.Get()));
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Image)));
		}
	}

	TEST_METHOD(ReferenceModesAcceptLocalAddressAndPointerLoad)
	{
		for (asDWORD Mode : {asDWORD(asTM_INREF), asDWORD(asTM_OUTREF), asDWORD(asTM_INOUTREF)})
		{
			for (asEBCInstr Push : {asBC_PSF, asBC_PshVPtr})
			{
				FDetachedDefinitionFixture Fixture;
				asCScriptFunction* Callee = DefineCallee(Fixture, ttInt, Mode);
				asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
				ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
				TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, Push, ByteLocal(2), true);
				ASSERT_THAT(IsNotNull(Image.Get()));
				ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
			}
		}
	}

	TEST_METHOD(UnknownZeroAndOversizedStorageReject)
	{
		for (asUINT Width : {0u, 0x7fffffffu})
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Callee = DefineCallee(Fixture, ttDouble);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(0));
			ASSERT_THAT(IsNotNull(Image.Get()));
			asSByteCodeRequirement* Requirement = FindRequirement(*Image, Callee->GetStableKey());
			ASSERT_THAT(IsNotNull(Requirement));
			Requirement->Parameters[0].StorageDwords = Width;
			Requirement->Parameters[0].StorageKind = asEByteCodeStorageKind::Value;
			asEncodeByteCodeRequirementContract(*Requirement, Requirement->ContractWitness);
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Image)));
		}
	}

	TEST_METHOD(TamperedWidthCannotAuthenticateAtTarget)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Callee = DefineCallee(Fixture, ttDouble);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC4, ByteImm32(0));
		ASSERT_THAT(IsNotNull(Image.Get()));
		asSByteCodeRequirement* Requirement = FindRequirement(*Image, Callee->GetStableKey());
		ASSERT_THAT(IsNotNull(Requirement));
		Requirement->Parameters[0].StorageDwords = 1;
		Requirement->Parameters[0].StorageKind = asEByteCodeStorageKind::Value;
		asEncodeByteCodeRequirementContract(*Requirement, Requirement->ContractWitness);
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Callee->GetStableKey(), asFUNCTION(TwiceDouble), asCALL_GENERIC)));
		asCExecutableSnapshot* Snapshot = nullptr;
		const asEByteCodeLinkStatus Status = Engine.Ptr->LinkByteCodeImage(*Image, Snapshot);
		if (Snapshot) Snapshot->Release();
		ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, Status));
		ASSERT_THAT(IsNull(Run->scriptData));
	}

	TEST_METHOD(DecodedRealDoubleCallReturnsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Parameter;
		Parameter.Name = "value";
		Parameter.Type = Fixture.Image->Primitive(ttDouble);
		asCScriptFunction* Callee = Fixture.DefineFunction(TEXT("Twice"), TEXT(""), asFUNC_SYSTEM,
			Fixture.Image->Primitive(ttDouble), MakeArrayView(&Parameter, 1));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttDouble), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		int64 Bits = 0;
		const double TwentyOne = 21.0;
		FMemory::Memcpy(&Bits, &TwentyOne, sizeof(Bits));
		TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(Bits));
		ASSERT_THAT(IsNotNull(Image.Get()));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Callee->GetStableKey(), asFUNCTION(TwiceDouble), asCALL_GENERIC)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Decoded, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		const int Result = Context->Execute();
		const double Value = Context->GetReturnDouble();
		Context->Release();
		Snapshot->Release();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Result));
		ASSERT_THAT(IsTrue(Value == 42.0)); // Literal 42 is exactly representable.
	}

	TEST_METHOD(EqualHeightAddressValueMergeRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		const asUINT Else = Builder.NewLabel(), Join = Builder.NewLabel();
		asSByteCodeOperand Branch;
		Branch.Kind = asEByteCodeOperandKind::Label;
		Branch.Label = Else;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JZ, MakeArrayView(&Branch, 1))));
		asSByteCodeOperand Address = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Address, 1))));
		Branch.Label = Join;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&Branch, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Else)));
		asSByteCodeOperand Scalar = ByteImm64(17);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC8, MakeArrayView(&Scalar, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Join)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopPtr, {})));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidJump, asCByteCodeVerifier::Verify(*Image)));
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreNotEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(MixedArgumentsAndReceiverConsumeExactStorage)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Owner = Fixture.DefineType(TEXT("Receiver"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asSMetadataParameter Params[2];
		Params[0].Name = "wide";
		Params[0].Type = Fixture.Image->Primitive(ttDouble);
		Params[1].Name = "reference";
		Params[1].Type = Fixture.Image->Primitive(ttInt);
		Params[1].Type.MakeReference(true);
		Params[1].Flags = asTM_INOUTREF;
		asCScriptFunction* Method = Fixture.DefineFunction(TEXT("Take"), TEXT(""), asFUNC_SYSTEM,
			Fixture.Image->Primitive(ttVoid), MakeArrayView(Params, 2), Owner);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
		ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT MethodSlot = 0, RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Method, MethodSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		const asUINT Start = Builder.NewLabel();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BindLabel(Start)));
		asSByteCodeOperand Ref = ByteLocal(2), Wide = ByteImm64(0), Self = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Ref, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC8, MakeArrayView(&Wide, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Self, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, MethodSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Back;
		Back.Kind = asEByteCodeOperandKind::Label;
		Back.Label = Start;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&Back, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::Succeeded, asCByteCodeVerifier::Verify(*Image)));
		Image->Functions[0].Instructions[2].Opcode = asBC_PshC8;
		Image->Functions[0].Instructions[2].Operands[0] = ByteImm64(123);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Image)));
	}

	TEST_METHOD(InvalidCategoryModeAndAggregateWidthRejectDirectAndDecoded)
	{
		for (int32 Mutation = 0; Mutation < 3; ++Mutation)
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Callee = DefineCallee(Fixture, ttDouble);
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(0));
			ASSERT_THAT(IsNotNull(Image.Get()));
			asSByteCodeRequirement* Requirement = FindRequirement(*Image, Callee->GetStableKey());
			ASSERT_THAT(IsNotNull(Requirement));
			if (Mutation == 0) Requirement->Parameters[0].StorageKind = asEByteCodeStorageKind::Unknown;
			if (Mutation == 1) Requirement->Parameters[0].PassingMode = 255;
			if (Mutation == 2)
			{
				Requirement->Parameters[0].StorageDwords = 32768;
				const asSByteCodeParameterContract Wide = Requirement->Parameters[0];
				Requirement->Parameters.Add(Wide);
				Requirement->Parameters.Add(Wide);
			}
			asEncodeByteCodeRequirementContract(*Requirement, Requirement->ContractWitness);
			ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::InvalidOperand, asCByteCodeVerifier::Verify(*Image)));
			TArray<uint8> Bytes;
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
			TUniquePtr<asCByteCodeImage> Decoded;
			ASSERT_THAT(AreNotEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
			ASSERT_THAT(IsNull(Decoded.Get()));
		}
	}

	TEST_METHOD(TamperedReturnStorageRejectsBeforePublication)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Callee = DefineCallee(Fixture, ttDouble);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = CallImage(*Callee, *Run, asBC_PshC8, ByteImm64(0));
		ASSERT_THAT(IsNotNull(Image.Get()));
		asSByteCodeRequirement* Requirement = FindRequirement(*Image, Callee->GetStableKey());
		ASSERT_THAT(IsNotNull(Requirement));
		Requirement->ReturnDwords = 2;
		Requirement->ReturnStorageKind = asEByteCodeStorageKind::Value;
		asEncodeByteCodeRequirementContract(*Requirement, Requirement->ContractWitness);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Callee->GetStableKey(), asFUNCTION(TwiceDouble), asCALL_GENERIC)));
		asCExecutableSnapshot* Snapshot = nullptr;
		const asEByteCodeLinkStatus Status = Engine.Ptr->LinkByteCodeImage(*Decoded, Snapshot);
		if (Snapshot) Snapshot->Release();
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, Status));
		ASSERT_THAT(IsNull(Run->scriptData));
	}

	TEST_METHOD(SwapPtrRequiresTwoCompletePointers)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttVoid), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		asSByteCodeOperand Scalar = ByteImm32(9), Address = ByteLocal(2), Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Scalar, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Address, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SwapPtr, {})));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		const asSByteCodeVerifyFault Fault = asCByteCodeVerifier::VerifyWithFault(*Image);
		ASSERT_THAT(AreEqual(asEByteCodeVerifyStatus::StackUnderflow, Fault.Status));
		ASSERT_THAT(AreEqual(2, Fault.InstructionIndex));
	}
};

#endif
