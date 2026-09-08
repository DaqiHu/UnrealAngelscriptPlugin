#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "../Definitions/NativeDetachedDefinitionTestSupport.h"
#include "as_bytecode_image_builder.h"
#include "as_bytecode_image_codec.h"
#include "as_bytecode_opcode.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMByteCodeImage, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSByteCodeOperand Imm32(int32 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS32;
		Operand.Immediate = Value;
		return Operand;
	}
	static asSByteCodeOperand Imm16(int16 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS16;
		Operand.Immediate = Value;
		return Operand;
	}
	static asSByteCodeOperand Label(asUINT Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::Label;
		Operand.Label = Value;
		return Operand;
	}
	static bool EmitReturn42(asCByteCodeImageBuilder& Builder, const asSStableKey& Key, int16 Pop = 0)
	{
		if (Builder.BeginFunction(Key, Key, 0, Pop) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand Push = Imm32(42);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand Ret = Imm16(Pop);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		return Builder.EndFunction() == asEByteCodeImageStatus::Succeeded;
	}

public:
	TEST_METHOD(RoundTripAndCanonicalOrder)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Type);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder First;
		asUINT TypeSlot = 0;
		asUINT FunctionSlot = 0;
		asUINT StringSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddFunctionRequirement(*Function, FunctionSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.AddString(TEXT("hello"), StringSlot)));
		ASSERT_THAT(IsTrue(EmitReturn42(First, Function->GetStableKey(), AS_PTR_SIZE)));
		TUniquePtr<asCByteCodeImage> FirstImage;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, First.Finish(FirstImage)));

		asCByteCodeImageBuilder Second;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddString(TEXT("hello"), StringSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddFunctionRequirement(*Function, FunctionSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(IsTrue(EmitReturn42(Second, Function->GetStableKey(), AS_PTR_SIZE)));
		TUniquePtr<asCByteCodeImage> SecondImage;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Second.Finish(SecondImage)));

		TArray<uint8> FirstBytes;
		TArray<uint8> SecondBytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*FirstImage, FirstBytes)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*SecondImage, SecondBytes)));
		ASSERT_THAT(AreEqual(FirstBytes.Num(), SecondBytes.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(FirstBytes.GetData(), SecondBytes.GetData(), FirstBytes.Num()) == 0));

		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(FirstBytes, Decoded)));
		ASSERT_THAT(AreEqual(FirstImage->Functions.Num(), Decoded->Functions.Num()));
		ASSERT_THAT(IsTrue(FirstImage->Functions[0].FunctionKey == Decoded->Functions[0].FunctionKey));
		FString Dump;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Dump(*Decoded, Dump)));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("req"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("0x")) && Dump.Contains(TEXT("live"))));
	}

	TEST_METHOD(LabelLoopAndFunctionOrder)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* First = Fixture.DefineFunction(TEXT("F"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		asCScriptFunction* Second = Fixture.DefineFunction(TEXT("G"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		auto Build = [&](bool bReverse) -> TArray<uint8>
		{
			asCByteCodeImageBuilder Builder;
			auto EmitFn = [&](asCScriptFunction* Function)
			{
				Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 1, ByteArgumentDwords(*Function));
				const asUINT Top = Builder.NewLabel();
				const asUINT Done = Builder.NewLabel();
				Builder.BindLabel(Top);
				asSByteCodeOperand Jump = Label(Done);
				Builder.Emit(asBC_JMP, MakeArrayView(&Jump, 1));
				Builder.BindLabel(Done);
				asSByteCodeOperand Ret = Imm16(ByteArgumentDwords(*Function));
				Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
				Builder.EndFunction();
			};
			if (bReverse) { EmitFn(Second); EmitFn(First); }
			else { EmitFn(First); EmitFn(Second); }
			TUniquePtr<asCByteCodeImage> Image;
			Builder.Finish(Image);
			TArray<uint8> Bytes;
			asCByteCodeImageCodec::Encode(*Image, Bytes);
			return Bytes;
		};
		const TArray<uint8> Forward = Build(false);
		const TArray<uint8> Reverse = Build(true);
		ASSERT_THAT(AreEqual(Forward.Num(), Reverse.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Forward.GetData(), Reverse.GetData(), Forward.Num()) == 0));
	}

	TEST_METHOD(GoldenHeaderAndRejections)
	{
		asCByteCodeImage Empty;
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(Empty, Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() >= 16));
		ASSERT_THAT(AreEqual(static_cast<uint8>('A'), Bytes[0]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('S'), Bytes[1]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('B'), Bytes[2]));
		ASSERT_THAT(AreEqual(static_cast<uint8>('C'), Bytes[3]));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Bytes[4])));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Bytes[5])));
		TUniquePtr<asCByteCodeImage> Decoded;
		TArray<uint8> Truncated;
		Truncated.Append(Bytes.GetData(), 8);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Truncated, asCByteCodeImageCodec::Decode(Truncated, Decoded)));
		TArray<uint8> Unknown = Bytes;
		Unknown[4] = 99;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::UnknownVersion, asCByteCodeImageCodec::Decode(Unknown, Decoded)));
		asCByteCodeImageBuilder Builder;
		asSByteCodeOperand Push = Imm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidArgument, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::InvalidArgument, Builder.BeginFunction({}, {}, 0, 0)));
		uint8 KeyBytes[32] = {};
		KeyBytes[0] = 1;
		asSStableKey Key;
		asSStableKey::TryCreate(MakeArrayView(KeyBytes, 32), Key);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Key, Key, 0, 0)));
		asSByteCodeOperand Str;
		Str.Kind = asEByteCodeOperandKind::ImmediateS32;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::UnknownOpcode, Builder.Emit(asBC_STR, MakeArrayView(&Str, 1))));
		const asUINT Missing = Builder.NewLabel();
		asSByteCodeOperand Jump = Label(Missing);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JMP, MakeArrayView(&Jump, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::MissingLabel, Builder.EndFunction()));
	}

	TEST_METHOD(HandleCycleRequirementTerminates)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* A = Fixture.DefineType(TEXT("A"), TEXT("cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectType* B = Fixture.DefineType(TEXT("B"), TEXT("cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* Unused = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddProperty(*A, "Other", asCDataType::CreateObjectHandle(B, false), Unused)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddProperty(*B, "Other", asCDataType::CreateObjectHandle(A, false), Unused)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*A, Slot)));
		asSStableKey Key;
		uint8 Bytes[32] = {};
		Bytes[0] = 3;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Key, Key, 0, 0)));
		asSByteCodeOperand Ret = Imm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		ASSERT_THAT(IsTrue(Image->Requirements.Num() >= 2));
	}
};

#endif
