#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_thread.h"

#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GCtxDtorCount = 0;
	asCScriptFunction* GCtxInner = nullptr;
	asCScriptEngine* GCtxEngine = nullptr;
	int32 GCtxLineHits = 0;

	void GenericCtxDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GCtxDtorCount;
	}

	void GenericCtxSuspend(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->Suspend();
		}
	}

	void GenericCtxNested(asIScriptGeneric* Generic)
	{
		asIScriptContext* Outer = asGetActiveContext();
		if (!Outer || !GCtxInner)
		{
			Generic->SetReturnDWord(0);
			return;
		}
		if (Outer->PushState() != 0 || Outer->Prepare(GCtxInner) != 0 || Outer->Execute() != asEXECUTION_FINISHED)
		{
			Outer->PopState();
			Generic->SetReturnDWord(0);
			return;
		}
		const asDWORD InnerResult = Outer->GetReturnDWord();
		Outer->PopState();
		Generic->SetReturnDWord(InnerResult);
	}

	void GenericCtxAbortInner(asIScriptGeneric* Generic)
	{
		asIScriptContext* Outer = asGetActiveContext();
		if (!GCtxEngine || !GCtxInner)
		{
			Generic->SetReturnDWord(0);
			return;
		}
		asIScriptContext* Inner = GCtxEngine->CreateContext();
		Inner->Prepare(GCtxInner);
		Inner->Execute();
		Inner->Abort();
		const int32 Dtors = GCtxDtorCount;
		Inner->Prepare(GCtxInner);
		Inner->Release();
		Generic->SetReturnDWord(asGetActiveContext() == Outer && Dtors >= 1 ? 42 : 0);
	}

	void GenericCtxShutdown(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (GCtxEngine)
		{
			GCtxEngine->ShutDownAndRelease();
		}
	}

	void GenericCtxReentrant(asIScriptGeneric* Generic)
	{
		asIScriptContext* Context = asGetActiveContext();
		Generic->SetReturnDWord(static_cast<asDWORD>(Context->Execute()));
	}

	void CtxLineCallback(asCContext* Context)
	{
		(void)Context;
		++GCtxLineHits;
	}
}

TEST_CLASS_WITH_FLAGS(VMContexts, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IndependentContextsAndReprepare)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* First = Engine.Ptr->CreateContext();
		asIScriptContext* Second = Engine.Ptr->CreateContext();
		ASSERT_THAT(IsTrue(First != Second));
		ASSERT_THAT(AreEqual(0, First->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, First->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(First->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, First->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, First->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(First->GetReturnDWord())));
		ASSERT_THAT(AreEqual(0, Second->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Second->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Second->GetReturnDWord())));
		First->Release();
		Second->Release();
		Snapshot->Release();
	}

	TEST_METHOD(SuspendResumeAddsOneThenTwoReturnsThree)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Suspend = Fixture.DefineFunction(TEXT("Yield"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Suspend->GetStableKey(), asFUNCTION(GenericCtxSuspend), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT YieldSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Suspend, YieldSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		asSByteCodeOperand Acc = ByteLocal(1);
		asSByteCodeOperand One = ByteImm32(1);
		asSByteCodeOperand SetOne[2] = {Acc, One};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOne, 2))));
		asSByteCodeOperand Yield = ByteSlot(asEByteCodeOperandKind::FunctionSlot, YieldSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Yield, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Three = ByteImm32(3);
		asSByteCodeOperand SetThree[2] = {Acc, Three};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetThree, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_SUSPENDED, Context->Execute()));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NestedNativeToScriptReturnsSeven)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Nested = Fixture.DefineFunction(TEXT("Nested"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Inner = Fixture.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Nested->GetStableKey(), asFUNCTION(GenericCtxNested), asCALL_GENERIC)));
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
		GCtxInner = Inner;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(Context->GetReturnDWord())));
		GCtxInner = nullptr;
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NestedInnerAbortRestoresOuter)
	{
		GCtxDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Live"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCDataType Handle = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Inner = Fixture.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, Handle, {});
		asCScriptFunction* AbortInner = Fixture.DefineFunction(TEXT("AbortInner"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericCtxDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(AbortInner->GetStableKey(), asFUNCTION(GenericCtxAbortInner), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT InnerSlot = 0;
		asUINT AbortSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Inner, InnerSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*AbortInner, AbortSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Inner->GetStableKey(), Inner->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AbortSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		GCtxEngine = Engine.Ptr;
		GCtxInner = Inner;
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(IsTrue(asGetActiveContext() != Context));
		GCtxEngine = nullptr;
		GCtxInner = nullptr;
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ThrowExceptionUnwindsOnce)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Kind = ByteImm16(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ThrowException, MakeArrayView(&Kind, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		ASSERT_THAT(IsNotNull(Context->GetExceptionString()));
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, Context->Execute()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(JitEntryAndSaveReturnValueAdvance)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_JitEntry, TConstArrayView<asSByteCodeOperand>())));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SaveReturnValue, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Acc = ByteLocal(1);
		asSByteCodeOperand FortyTwo = ByteImm32(42);
		asSByteCodeOperand SetOps[2] = {Acc, FortyTwo};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SetV4, MakeArrayView(SetOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Acc, 1))));
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
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ReentrantExecuteIsRejected)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Reenter = Fixture.DefineFunction(TEXT("Reenter"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Reenter->GetStableKey(), asFUNCTION(GenericCtxReentrant), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT ReenterSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Reenter, ReenterSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 4, 0)));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, ReenterSlot);
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
		ASSERT_THAT(AreEqual(static_cast<int32>(asCONTEXT_ACTIVE), static_cast<int32>(Context->GetReturnDWord())));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ShutdownRejectsPrepareAndKeepsMetadataReadable)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		Engine.Ptr->ShutDownAndRelease();
		Engine.Ptr = nullptr;
		ASSERT_THAT(IsTrue(Context->Prepare(Function) != 0));
		ASSERT_THAT(IsNotNull(Function->GetName()));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(IndependentThreadsPreserveResults)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* First = Engine.Ptr->CreateContext();
		asIScriptContext* Second = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, First->Prepare(Function)));
		ASSERT_THAT(AreEqual(0, Second->Prepare(Function)));
		int32 FirstResult = 0;
		int32 SecondResult = 0;
		std::thread Thread([&]()
		{
			SecondResult = Second->Execute();
		});
		FirstResult = First->Execute();
		Thread.join();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, FirstResult));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, SecondResult));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(First->GetReturnDWord())));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Second->GetReturnDWord())));
		First->Release();
		Second->Release();
		Snapshot->Release();
	}
};

#endif
