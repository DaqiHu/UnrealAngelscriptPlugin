#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_scriptobject.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GGcDtorCount = 0;

	void GenericGcDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GGcDtorCount;
	}

	void GenericGcCollect(asIScriptGeneric* Generic)
	{
		if (asIScriptEngine* Engine = Generic->GetEngine())
		{
			Engine->GarbageCollect(asGC_FULL_CYCLE);
		}
	}

	asCObjectType* DefineGcNode(FDetachedDefinitionFixture& Fixture, const TCHAR* Name, asCScriptFunction*& OutDtor)
	{
		asCObjectType* Type = Fixture.DefineType(Name, TEXT(""), asOBJ_REF | asOBJ_GC | asOBJ_SCRIPT_OBJECT);
		if (!Type) return nullptr;
		asCObjectProperty* Next = nullptr;
		if (Fixture.Set->AddProperty(*Type, "Next", asCDataType::CreateObjectHandle(Type, false), Next) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		OutDtor = Fixture.DefineFunction(TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {}, Type);
		if (!OutDtor) return nullptr;
		if (Fixture.Set->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *OutDtor) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Type;
	}

	void DisableGcAutoGc(asCScriptEngine* Engine)
	{
		Engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, 0);
	}
}

TEST_CLASS_WITH_FLAGS(VMGC, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(EngineGarbageCollectAfterExecution)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
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
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		asUINT Current = 0;
		Engine.Ptr->GetGCStatistics(&Current, nullptr, nullptr, nullptr, nullptr);
		Engine.Ptr->GarbageCollect();
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(UnrootedSelfCycleFinalizesOnce)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("SelfNode"), Dtor);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
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
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(UnrootedTwoNodeCycleFinalizesEachOnce)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("PairNode"), Dtor);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Left = ByteLocal(2);
		asSByteCodeOperand Right = ByteLocal(4);
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Left, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Right, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		asSByteCodeOperand Offset = ByteImm16(static_cast<int16>(NextOffset));
		asSByteCodeOperand RefType = ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Right, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Left, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Offset, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_REFCPY, MakeArrayView(&RefType, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Left, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Right, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ADDSi, MakeArrayView(&Offset, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_REFCPY, MakeArrayView(&RefType, 1))));
		asSByteCodeOperand FreeLeft[2] = {Left, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		asSByteCodeOperand FreeRight[2] = {Right, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeLeft, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeRight, 2))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(2, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RootedCycleSurvivesThenDies)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("RootNode"), Dtor);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
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
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Root = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Root));
		Engine.Ptr->AddRefScriptObject(Root, Type);
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->ReleaseScriptObject(Root, Type);
		Engine.Ptr->ReleaseScriptObject(Root, Type);
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(WeakRefValidThenInvalidAfterCollection)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("WeakNode"), Dtor);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
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
		asILockableSharedBool* Flag = Engine.Ptr->GetWeakRefFlagOfScriptObject(Object, Type);
		ASSERT_THAT(IsNotNull(Flag));
		ASSERT_THAT(IsFalse(Flag->Get()));
		asILockableSharedBool* Copied = Engine.Ptr->GetWeakRefFlagOfScriptObject(Object, Type);
		ASSERT_THAT(IsTrue(Copied == Flag));
		Engine.Ptr->ReleaseScriptObject(Object, Type);
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		ASSERT_THAT(IsTrue(Flag->Get()));
		ASSERT_THAT(IsTrue(Copied->Get()));
		Flag->Release();
		Copied->Release();
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(RefCpyVBalancesAddRefRelease)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("CopyNode"), Dtor);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Src = ByteLocal(2);
		asSByteCodeOperand Dst = ByteLocal(4);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Src, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ClrVPtr, MakeArrayView(&Dst, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&Src, 1))));
		asSByteCodeOperand CopyOps[2] = {Dst, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RefCpyV, MakeArrayView(CopyOps, 2))));
		asSByteCodeOperand FreeSrc[2] = {Src, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeSrc, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Dst, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Copied = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Copied));
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->ReleaseScriptObject(Copied, Type);
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(PreparedContextRootsLiveObject)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("LiveNode"), Dtor);
		asCScriptFunction* Collect = Fixture.DefineFunction(TEXT("Collect"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttVoid), {});
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Collect->GetStableKey(), asFUNCTION(GenericGcCollect), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT CollectSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Collect, CollectSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, CollectSlot);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1))));
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(FullAndIncrementalReachSameDestructorSet)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("StepNode"), Dtor);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
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
		asSByteCodeOperand FreeOps[2] = {Obj, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2))));
		asSByteCodeOperand Zero = ByteImm32(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Zero, 1))));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		int32 Steps = 0;
		while (Steps < 1024 && GGcDtorCount == 0)
		{
			Engine.Ptr->GarbageCollect(asGC_ONE_STEP);
			++Steps;
		}
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(DelegateCaptureCycleIsReclaimed)
	{
		GGcDtorCount = 0;
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Dtor = nullptr;
		asCObjectType* Type = DefineGcNode(Fixture, TEXT("DelegNode"), Dtor);
		asCScriptFunction* Value = Fixture.DefineFunction(TEXT("Value"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {}, Type);
		asCDataType ReturnType = asCDataType::CreateObjectHandle(Type, false);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsNotNull(Type));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		const int32 NextOffset = Type->localProperties.GetLength() ? Type->localProperties[0]->byteOffset : 0;
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		DisableGcAutoGc(Engine.Ptr);
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericGcDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		asUINT ValueSlot = 0;
		Builder.AddFunctionRequirement(*Value, ValueSlot);
		Builder.BeginFunction(Value->GetStableKey(), Value->GetStableKey(), 4, ByteArgumentDwords(*Value));
		asSByteCodeOperand FortyTwo = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&FortyTwo, 1));
		asSByteCodeOperand Ret = ByteImm16(ByteArgumentDwords(*Value));
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, ByteArgumentDwords(*Run))));
		asSByteCodeOperand Obj = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_LOADOBJ, MakeArrayView(&Obj, 1))));
		asSByteCodeOperand RunRet = ByteImm16(ByteArgumentDwords(*Run));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&RunRet, 1))));
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
		asIScriptFunction* Delegate = Engine.Ptr->CreateDelegate(Value, Object);
		ASSERT_THAT(IsNotNull(Delegate));
		void** Slot = reinterpret_cast<void**>(static_cast<char*>(Object) + NextOffset);
		*Slot = Delegate;
		Delegate->AddRef();
		Engine.Ptr->ReleaseScriptObject(Object, Type);
		Delegate->Release();
		ASSERT_THAT(AreEqual(0, GGcDtorCount));
		Engine.Ptr->GarbageCollect(asGC_FULL_CYCLE);
		ASSERT_THAT(AreEqual(1, GGcDtorCount));
		Context->Release();
		Snapshot->Release();
	}
};

#endif
