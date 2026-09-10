#include "CQTest.h"
#include "NativeVMTestSupport.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMLinking, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LinkResolvesOriginalFunctionPointers)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0)));
		asSByteCodeOperand Push = ByteImm32(42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNotNull(Snapshot));
		ASSERT_THAT(IsTrue(Snapshot->FindFunction(Function->GetStableKey()) == Function));
		Snapshot->Release();
	}

	TEST_METHOD(MissingDeclarationAndBodyConflictReject)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Push = ByteImm32(1);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		FNativeEngineOwner Engine;
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asCExecutableSnapshot* Second = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::BodyConflict, Engine.Ptr->LinkByteCodeImage(*Image, Second)));
		ASSERT_THAT(IsNull(Second));
		Snapshot->Release();
	}
};

#endif
