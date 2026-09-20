#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_vm_object.h"

#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GRootDtor = 0;
	int32 GRootCtor = 0;
	asCScriptFunction* GRootInner = nullptr;
	asCScriptEngine* GRootEngine = nullptr;
	asCObjectType* GRootType = nullptr;

	void GenericRootDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GRootDtor;
	}

	void GenericRootConstruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GRootCtor;
	}

	void GenericRootThrow(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->SetException("ctor-fail");
		}
	}

	void GenericRootSuspend(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->Suspend();
		}
	}

	void GenericRootCollect(asIScriptGeneric* Generic)
	{
		if (asIScriptEngine* Engine = Generic->GetEngine())
		{
			Engine->GarbageCollect(asGC_FULL_CYCLE);
			Engine->GarbageCollect(asGC_ONE_STEP);
		}
	}

	asCObjectType* DefineRootNode(FDetachedDefinitionFixture& Fixture, const TCHAR* Name, asCScriptFunction*& OutDtor)
	{
		asCObjectType* Type = Fixture.DefineType(Name, TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		if (!Type) return nullptr;
		asCObjectProperty* Next = nullptr;
		if (Fixture.Set->AddProperty(*Type, "Next", asCDataType::CreateObjectHandle(Type, false), Next) != asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		OutDtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		if (!OutDtor) return nullptr;
		if (Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *OutDtor) != asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		return Type;
	}

	void DisableRootLifetimeAutoGc(asCScriptEngine* Engine)
	{
		Engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
	}
}

TEST_CLASS_WITH_FLAGS(VMRootLifetime, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SuspendedCycleSurvivesGcThenAbortFinalizesOnce)
	{
		GRootDtor = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineRootNode(Fixture, TEXT("YieldNode"), Dtor);
		asCScriptFunction* Yield = Fixture.DefineFunction(TEXT("Yield"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableRootLifetimeAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericRootDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Yield->GetStableKey(), asFUNCTION(GenericRootSuspend), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT YieldSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Yield, YieldSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Offset = ByteImm16(static_cast<int16>(NextOffset));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Offset, 1))));
		asSByteCodeOperand RefType = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_REFCPY, MakeArrayView(&RefType, 1))));
		asSByteCodeOperand YieldCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, YieldSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&YieldCall, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_SUSPEND, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
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
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Engine.Ptr->GarbageCollect(asGC_ONE_STEP);
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Context->Abort();
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GRootDtor));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(ConcurrentAddRefReleaseFinalizesOnce)
	{
		GRootDtor = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineRootNode(Fixture, TEXT("RaceNode"), Dtor);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableRootLifetimeAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericRootDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
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
		void* Object = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Object));
		Engine.Ptr->AddRefScriptObject(Object, Type);
		std::thread First([&]()
		{
			for (int32 Index = 0; Index < 1000; ++Index)
			{
				Engine.Ptr->AddRefScriptObject(Object, Type);
				Engine.Ptr->ReleaseScriptObject(Object, Type);
			}
		});
		std::thread Second([&]()
		{
			for (int32 Index = 0; Index < 1000; ++Index)
			{
				Engine.Ptr->AddRefScriptObject(Object, Type);
				Engine.Ptr->ReleaseScriptObject(Object, Type);
			}
		});
		First.join();
		Second.join();
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Engine.Ptr->ReleaseScriptObject(Object, Type);
		Engine.Ptr->ReleaseScriptObject(Object, Type);
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GRootDtor));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FailedConstructorIsNotCollectedAsComplete)
	{
		GRootDtor = 0;
		GRootCtor = 0;
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Broken"), TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Ctor = Fixture.DefineFunction(
			TEXT("Construct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_CONSTRUCT, *Ctor)));
		ASSERT_THAT(AreEqual(asEDefinitionResult::Succeeded, Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableRootLifetimeAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Ctor->GetStableKey(), asFUNCTION(GenericRootThrow), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericRootDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT CtorSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Ctor, CtorSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand PtrLocal = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&PtrLocal, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&PtrLocal, 1))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, CtorSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
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
		ASSERT_THAT(AreEqual(0, GRootCtor));
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(TwoEnginesKeepIndependentRoots)
	{
		GRootDtor = 0;
		FDetachedDefinitionFixture FixtureA;
		asCScriptFunction* DtorA = nullptr;
		asCObjectType* TypeA = DefineRootNode(FixtureA, TEXT("EngineANode"), DtorA);
		asCDataType ReturnA = asCDataType::CreateObjectHandle(TypeA, false);
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnA, {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		FDetachedDefinitionFixture FixtureB;
		asCScriptFunction* DtorB = nullptr;
		asCObjectType* TypeB = DefineRootNode(FixtureB, TEXT("EngineBNode"), DtorB);
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineA.Ptr->RegisterExternalDefinitions(FixtureA.TakeSet())));
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineB.Ptr->RegisterExternalDefinitions(FixtureB.TakeSet())));
		DisableRootLifetimeAutoGc(EngineA.Ptr);
		DisableRootLifetimeAutoGc(EngineB.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineA.Ptr->BindNativeFunction(DtorA->GetStableKey(), asFUNCTION(GenericRootDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*TypeA, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*RunA, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(RunA->GetStableKey(), RunA->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*EngineA.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = EngineA.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(RunA)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Root = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Root));
		EngineA.Ptr->AddRefScriptObject(Root, TypeA);
		EngineB.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(0, GRootDtor));
		EngineA.Ptr->ReleaseScriptObject(Root, TypeA);
		EngineA.Ptr->ReleaseScriptObject(Root, TypeA);
		EngineA.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GRootDtor));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(NestedNativeCollectLeavesInnerRootUntilUnwind)
	{
		GRootDtor = 0;
		GRootInner = nullptr;
		GRootEngine = nullptr;
		GRootType = nullptr;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineRootNode(Fixture, TEXT("NestedNode"), Dtor);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Inner = Fixture.DefineFunction(TEXT("Inner"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		asCScriptFunction* Collect = Fixture.DefineFunction(TEXT("Collect"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableRootLifetimeAutoGc(Engine.Ptr);
		GRootEngine = Engine.Ptr;
		GRootType = Type;
		GRootInner = Inner;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericRootDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Collect->GetStableKey(), asFUNCTION(GenericRootCollect), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT InnerSlot = 0;
		asUINT CollectSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Inner, InnerSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Collect, CollectSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Inner->GetStableKey(), Inner->GetStableKey(), 16, 0)));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		asSByteCodeOperand CollectCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, CollectSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&CollectCall, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0)));
		asSByteCodeOperand InnerCall = ByteSlot(asEByteCodeOperandKind::FunctionSlot, InnerSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALL, MakeArrayView(&InnerCall, 1))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GRootDtor));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GRootDtor));
		Context->Release();
		Snapshot->Release();
		GRootInner = nullptr;
		GRootEngine = nullptr;
		GRootType = nullptr;
	}
};

#endif
