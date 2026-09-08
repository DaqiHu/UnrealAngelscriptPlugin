#include "CQTest.h"
#include "NativeVMTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMScalar, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExecuteConstantReturnFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, ByteArgumentDwords(*Function));
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ExecuteAddSevenAndThirtyFive)
	{
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter A;
		A.Name = "a";
		A.Type = Fixture.Image->Primitive(ttInt);
		asSMetadataParameter B;
		B.Name = "b";
		B.Type = Fixture.Image->Primitive(ttInt);
		asSMetadataParameter Params[2] = {A, B};
		asCScriptFunction* Function = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 8, ByteArgumentDwords(*Function));
		asSByteCodeOperand Ops[3];
		Ops[0].Kind = asEByteCodeOperandKind::Local;
		Ops[0].Immediate = 2;
		Ops[1].Kind = asEByteCodeOperandKind::Local;
		Ops[1].Immediate = 0;
		Ops[2].Kind = asEByteCodeOperandKind::Local;
		Ops[2].Immediate = -1;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDi, MakeArrayView(Ops, 3))));
		asSByteCodeOperand Copy;
		Copy.Kind = asEByteCodeOperandKind::Local;
		Copy.Immediate = 2;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Copy, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Function));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 7)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 35)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
