#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_callfunc.h"
#include "as_generic.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GOwnershipNativeCalls = 0;

	void OwnershipGenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GOwnershipNativeCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	asSMetadataParameter MakeOwnershipIntParam(const char* Name)
	{
		asSMetadataParameter Param;
		Param.Name = Name;
		return Param;
	}
}

TEST_CLASS_WITH_FLAGS(VMExecutableOwnership, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TUniquePtr<asCByteCodeImage> MakeReturn42(asCScriptFunction* Function)
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

public:
	TEST_METHOD(ScriptFortyTwoLeavesFrozenDeclarationUnchanged)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(IsNull(Function->scriptData));
		ASSERT_THAT(IsNull(Function->sysFuncIntf));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
		ASSERT_THAT(IsNotNull(Image.Get()));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		ASSERT_THAT(IsNotNull(Snapshot));
		ASSERT_THAT(IsTrue(Snapshot->FindFunction(Function->GetStableKey()) == Function));
		asCExecutableFunction* Executable = Snapshot->FindExecutable(Function->GetStableKey());
		ASSERT_THAT(IsNotNull(Executable));
		ASSERT_THAT(IsTrue(Executable->Declaration == Function));
		ASSERT_THAT(IsNotNull(Executable->scriptData));
		ASSERT_THAT(IsTrue(Executable->scriptData->byteCode.GetLength() > 0));
		ASSERT_THAT(IsNull(Function->scriptData));
		ASSERT_THAT(IsNull(Function->sysFuncIntf));
		asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(Function->scriptData == nullptr || Function->scriptData->byteCode.GetLength() == 0));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NativeTwentyPlusTwentyTwoLeavesSysFuncIntfUnchanged)
	{
		GOwnershipNativeCalls = 0;
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Params[2] = { MakeOwnershipIntParam("a"), MakeOwnershipIntParam("b") };
		Params[0].Type = Fixture.Set->Primitive(ttInt);
		Params[1].Type = Fixture.Set->Primitive(ttInt);
		asCScriptFunction* Add = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(Params, 2));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(IsNull(Add->sysFuncIntf));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(OwnershipGenericAdd), asCALL_GENERIC)));
		ASSERT_THAT(IsNull(Add->sysFuncIntf));
		asCByteCodeImageBuilder Builder;
		asUINT AddSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Add, AddSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		asSByteCodeOperand RightOp = ByteImm32(22);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&RightOp, 1))));
		asSByteCodeOperand LeftOp = ByteImm32(20);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&LeftOp, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsNull(Add->sysFuncIntf));
		ASSERT_THAT(IsNull(Run->scriptData));
		ASSERT_THAT(AreEqual(1, GOwnershipNativeCalls));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ForeignAndUnlinkedPrepareReject)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
		ASSERT_THAT(AreNotEqual(0, Context->Prepare(Function)));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreNotEqual(0, Context->Prepare(static_cast<asIScriptFunction*>(nullptr))));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(TwoEnginesKeepIndependentExecutableStorage)
	{
		FDetachedDefinitionFixture FirstFixture;
		asCScriptFunction* FirstFn = FirstFixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FirstFixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FirstFixture.FinalizeAndFreeze()));
		FDetachedDefinitionFixture SecondFixture;
		asCScriptFunction* SecondFn = SecondFixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, SecondFixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(SecondFixture.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(FirstFn->GetStableKey() == SecondFn->GetStableKey()));
		FNativeEngineOwner FirstEngine;
		FNativeEngineOwner SecondEngine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, FirstEngine.Ptr->InstallDefinitionSet(FirstFixture.TakeSet())));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, SecondEngine.Ptr->InstallDefinitionSet(SecondFixture.TakeSet())));
		TUniquePtr<asCByteCodeImage> FirstImage = MakeReturn42(FirstFn);
		TUniquePtr<asCByteCodeImage> SecondImage = MakeReturn42(SecondFn);
		asCExecutableSnapshot* FirstSnap = nullptr;
		asCExecutableSnapshot* SecondSnap = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*FirstEngine.Ptr, *FirstImage, FirstSnap)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*SecondEngine.Ptr, *SecondImage, SecondSnap)));
		asCExecutableFunction* FirstExec = FirstSnap->FindExecutable(FirstFn->GetStableKey());
		asCExecutableFunction* SecondExec = SecondSnap->FindExecutable(SecondFn->GetStableKey());
		ASSERT_THAT(IsNotNull(FirstExec));
		ASSERT_THAT(IsNotNull(SecondExec));
		ASSERT_THAT(IsTrue(FirstExec != SecondExec));
		ASSERT_THAT(IsTrue(FirstExec->scriptData != SecondExec->scriptData));
		asIScriptContext* FirstCtx = FirstEngine.Ptr->CreateContext();
		asIScriptContext* SecondCtx = SecondEngine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, FirstCtx->Prepare(FirstFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, FirstCtx->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(FirstCtx->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, SecondCtx->Prepare(SecondFn)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, SecondCtx->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(SecondCtx->GetReturnDWord())));
		FirstCtx->Release();
		SecondCtx->Release();
		FirstSnap->Release();
		SecondSnap->Release();
	}

	TEST_METHOD(FailedCandidateDoesNotInstallAndExistingBodySurvives)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Good = MakeReturn42(Function);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Good, Snapshot)));
		asCByteCodeImageBuilder BadBuilder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.AddFunctionRequirement(*Function, Slot)));
		uint8 MissingBytes[32] = {};
		MissingBytes[0] = 99;
		asSStableKey Missing;
		asSStableKey::TryCreate(MakeArrayView(MissingBytes, 32), Missing);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.BeginFunction(Missing, Missing, 4, 0)));
		asSByteCodeOperand Push = ByteImm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Bad;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, BadBuilder.Finish(Bad)));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Bad, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsTrue(Function->scriptData == nullptr || Function->scriptData->byteCode.GetLength() == 0));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ExecutionSurvivesProducerRelease)
	{
		FNativeEngineOwner Engine;
		asCExecutableSnapshot* Snapshot = nullptr;
		asCScriptFunction* Held = nullptr;
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
			TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
			Held = Snapshot->FindFunction(Function->GetStableKey());
			ASSERT_THAT(IsNotNull(Held));
		}
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Held)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
