#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_generic.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GLeaseNativeCalls = 0;

	void LeaseGenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GLeaseNativeCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	asSMetadataParameter MakeLeaseIntParam(const char* Name)
	{
		asSMetadataParameter Param;
		Param.Name = Name;
		return Param;
	}

	TUniquePtr<asCByteCodeImage> MakeLeaseReturn42(asCScriptFunction* Function)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Push = ByteImm32(42);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}
}

TEST_CLASS_WITH_FLAGS(VMExecutableLeases, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExecuteAfterCallerSnapshotReleaseReturnsFortyTwo)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		TUniquePtr<asCByteCodeImage> Image = MakeLeaseReturn42(Function);
		ASSERT_THAT(IsNotNull(Image.Get()));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNotNull(Snapshot));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		Snapshot->Release();
		Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
	}

	TEST_METHOD(NativePrepareExecuteAddTwentyAndTwentyTwo)
	{
		GLeaseNativeCalls = 0;
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Params[2] = { MakeLeaseIntParam("a"), MakeLeaseIntParam("b") };
		Params[0].Type = Fixture.Image->Primitive(ttInt);
		Params[1].Type = Fixture.Image->Primitive(ttInt);
		asCScriptFunction* Add = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(LeaseGenericAdd), asCALL_GENERIC)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Add)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(0, 20)));
		ASSERT_THAT(AreEqual(0, Context->SetArgDWord(1, 22)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GLeaseNativeCalls));
		Context->Release();
	}

	TEST_METHOD(MissingPublishedScriptPrepareReturnsNoFunction)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(asNO_FUNCTION, Context->Prepare(Function)));
		Context->Release();
	}
};

#endif
