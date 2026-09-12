#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_scriptfunction.h"
#include "as_thread.h"

#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GBoundLineHits = 0;
	int32 GBoundLineHitsB = 0;
	asCScriptFunction* GBoundInner = nullptr;

	void BoundLineCallback(asCContext* Context)
	{
		(void)Context;
		++GBoundLineHits;
	}

	void BoundLineCallbackB(asCContext* Context)
	{
		(void)Context;
		++GBoundLineHitsB;
	}

	void GenericBoundNested(asIScriptGeneric* Generic)
	{
		asIScriptContext* Outer = asGetActiveContext();
		if (!Outer || !GBoundInner)
		{
			Generic->SetReturnDWord(0);
			return;
		}
		if (Outer->PushState() != 0)
		{
			Generic->SetReturnDWord(0);
			return;
		}
		asDWORD Value = 0;
		if (Outer->Prepare(GBoundInner) == 0 && Outer->Execute() == asEXECUTION_FINISHED)
		{
			Value = Outer->GetReturnDWord();
		}
		Outer->PopState();
		Generic->SetReturnDWord(asGetActiveContext() == Outer ? Value : 0);
	}

	TUniquePtr<asCByteCodeImage> MakeConstantReturn(asCScriptFunction* Function, int32 Value)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>()) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand Acc = ByteLocal(1);
		asSByteCodeOperand Imm = ByteImm32(Value);
		asSByteCodeOperand SetOps[2] = {Acc, Imm};
		if (Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}
}

TEST_CLASS_WITH_FLAGS(VMContextBoundaries, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RecursionHitsStackLimitThenRecoveryNinetySeven)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Recurse = Fixture.DefineFunction(TEXT("Recurse"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Recovery = Fixture.DefineFunction(TEXT("Recovery"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		Engine.Ptr->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asCByteCodeImageBuilder Builder;
		asUINT RecurseSlot = 0;
		asUINT RecoverySlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Recurse, RecurseSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Recovery, RecoverySlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Recurse->GetStableKey(), Recurse->GetStableKey(), 16, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, RecurseSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Recovery->GetStableKey(), Recovery->GetStableKey(), 4, 0)));
		asSByteCodeOperand Push = ByteImm32(97);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Recurse)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsNotNull(Context->GetExceptionString()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Recovery)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(LineCallbackFiresWhenSetAndSilentWhenCleared)
	{
		GBoundLineHits = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeConstantReturn(Run, 42);
		ASSERT_THAT(IsNotNull(Image.Get()));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
		ASSERT_THAT(AreEqual(0, Context->SetLineCallback(&BoundLineCallback)));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		const int32 EnabledHits = GBoundLineHits;
		ASSERT_THAT(IsTrue(EnabledHits > 0));
		Context->ClearLineCallback();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(EnabledHits, GBoundLineHits));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NestedNativeToScriptRestoresOuterAndReturnsSeven)
	{
		GBoundInner = nullptr;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Nested = Fixture.DefineFunction(TEXT("Nested"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Inner = Fixture.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Nested->GetStableKey(), asFUNCTION(GenericBoundNested), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT NestedSlot = 0;
		asUINT InnerSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Nested, NestedSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Inner, InnerSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Inner->GetStableKey(), Inner->GetStableKey(), 4, 0)));
		asSByteCodeOperand Seven = ByteImm32(7);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Seven, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, NestedSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		GBoundInner = Inner;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(asGetActiveContext() != Context));
		GBoundInner = nullptr;
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(TwoThreadsKeepIndependentLineCallbacks)
	{
		GBoundLineHits = 0;
		GBoundLineHitsB = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeConstantReturn(Run, 42);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		std::thread First([&]()
		{
			asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
			Context->SetLineCallback(&BoundLineCallback);
			Context->Prepare(Run);
			Context->Execute();
			Context->Release();
		});
		std::thread Second([&]()
		{
			asCContext* Context = static_cast<asCContext*>(Engine.Ptr->CreateContext());
			Context->SetLineCallback(&BoundLineCallbackB);
			Context->Prepare(Run);
			Context->Execute();
			Context->Release();
		});
		First.join();
		Second.join();
		ASSERT_THAT(IsTrue(GBoundLineHits > 0));
		ASSERT_THAT(IsTrue(GBoundLineHitsB > 0));
		Snapshot->Release();
	}
};

#endif
