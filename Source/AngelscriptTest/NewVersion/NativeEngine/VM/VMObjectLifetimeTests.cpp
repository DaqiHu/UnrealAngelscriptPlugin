#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_vm_object.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GLifeCtor = 0;
	int32 GLifeDtor = 0;
	int32 GMemberDtor[2] = {0, 0};
	int32 GOuterDtor = 0;

	void ResetLifetimeTraces()
	{
		GLifeCtor = 0;
		GLifeDtor = 0;
		GMemberDtor[0] = 0;
		GMemberDtor[1] = 0;
		GOuterDtor = 0;
	}

	void GenericLifeConstruct(asIScriptGeneric* Generic)
	{
		int32* Payload = static_cast<int32*>(Generic->GetObject());
		*Payload = 17;
		++GLifeCtor;
	}

	void GenericLifeDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GLifeDtor;
	}

	void GenericMember0Destruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GMemberDtor[0];
	}

	void GenericMember1Destruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GMemberDtor[1];
	}

	void GenericOuterDestruct(asIScriptGeneric* Generic)
	{
		(void)Generic;
		++GOuterDtor;
	}

	asSMetadataParameter MakeLifeIntParam(const char* Name)
	{
		asSMetadataParameter Param;
		Param.Name = Name;
		return Param;
	}

	TUniquePtr<asCByteCodeImage> MakeAllocFreeImage(
		asCObjectType* Type,
		asCObjectProperty* Field,
		asCScriptFunction* Ctor,
		asCScriptFunction* Run)
	{
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT CtorSlot = 0;
		asUINT RunSlot = 0;
		if (Builder.AddTypeRequirement(*Type, TypeSlot) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Ctor && Builder.AddFunctionRequirement(*Ctor, CtorSlot) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.AddFunctionRequirement(*Run, RunSlot) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand PtrLocal = ByteLocal(2);
		if (Builder.Emit(asBC_PSF, MakeArrayView(&PtrLocal, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		if (Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Ctor)
		{
			if (Builder.Emit(asBC_PshVPtr, MakeArrayView(&PtrLocal, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
			asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, CtorSlot);
			if (Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		}
		asSByteCodeOperand Temp = ByteLocal(4);
		asSByteCodeOperand LoadOps[2] = {PtrLocal, ByteImm16(static_cast<int16>(Field->byteOffset))};
		if (Builder.Emit(asBC_LoadRObjR, MakeArrayView(LoadOps, 2)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.Emit(asBC_RDR4, MakeArrayView(&Temp, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.Emit(asBC_CpyVtoR4, MakeArrayView(&Temp, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand FreeOps[2] = {PtrLocal, ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot)};
		if (Builder.Emit(asBC_FREE, MakeArrayView(FreeOps, 2)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return nullptr;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return nullptr;
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded) return nullptr;
		return Image;
	}
}

TEST_CLASS_WITH_FLAGS(VMObjectLifetime, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DestructorFiresThroughNativeBinding)
	{
		ResetLifetimeTraces();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* X = Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* Ctor = Fixture.DefineFunction(
			TEXT("Construct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_CONSTRUCT, *Ctor)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Ctor->GetStableKey(), asFUNCTION(GenericLifeConstruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericLifeDestruct), asCALL_GENERIC)));
		TUniquePtr<asCByteCodeImage> Image = MakeAllocFreeImage(Type, X, Ctor, Run);
		ASSERT_THAT(IsNotNull(Image.Get()));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(Context->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GLifeCtor));
		ASSERT_THAT(AreEqual(1, GLifeDtor));
		Context->Release();
		Snapshot->Release();
	}

	TEST_METHOD(HighAlignmentHeaderRecovers)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Wide"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		Type->alignment = 1024;
		void* Payload = asVmAllocateObject(Type);
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(IsTrue((reinterpret_cast<asPWORD>(Payload) & 1023) == 0));
		asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
		ASSERT_THAT(IsNotNull(Header));
		ASSERT_THAT(IsTrue(Header->Type == Type));
		*static_cast<int32*>(Payload) = 17;
		ASSERT_THAT(AreEqual(17, *static_cast<int32*>(Payload)));
		asVmFreeObject(Payload);
		ASSERT_THAT(IsNull(asVmHeaderFromPayload(Payload)));
	}

	TEST_METHOD(NativeHandleIsNotProbedAsSdkHeader)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(ttInt));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		alignas(16) uint8 Native[64];
		FMemory::Memset(Native, 0xAB, sizeof(Native));
		*reinterpret_cast<asUINT*>(Native) = asSVmObjectHeader::MagicValue;
		void* FakePayload = Native + 16;
		*reinterpret_cast<void**>(static_cast<uint8*>(FakePayload) - sizeof(void*)) = Native;
		ASSERT_THAT(IsNull(asVmHeaderFromPayload(FakePayload)));
		asVmReleaseAllReferences(FakePayload, Engine.Ptr);
		ASSERT_THAT(AreEqual(0xAB, static_cast<int32>(Native[32])));
	}

	TEST_METHOD(PartialMemberFailureSkipsWholeObjectDestructor)
	{
		ResetLifetimeTraces();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Box = Fixture.DefineType(TEXT("Box"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Box, "tag", Fixture.Image->Primitive(ttInt));
		asCScriptFunction* BoxDtor = Fixture.DefineFunction(
			TEXT("BoxDestruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Box);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Box, asBEHAVE_DESTRUCT, *BoxDtor)));
		asCObjectType* Outer = Fixture.DefineType(TEXT("Outer"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCDataType BoxValue = asCDataType::CreateType(Box, false);
		Fixture.DefineProperty(*Outer, "first", BoxValue);
		Fixture.DefineProperty(*Outer, "second", BoxValue);
		asCScriptFunction* OuterDtor = Fixture.DefineFunction(
			TEXT("OuterDestruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Outer);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Outer, asBEHAVE_DESTRUCT, *OuterDtor)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(BoxDtor->GetStableKey(), asFUNCTION(GenericMember0Destruct), asCALL_GENERIC)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(OuterDtor->GetStableKey(), asFUNCTION(GenericOuterDestruct), asCALL_GENERIC)));
		void* Payload = asVmAllocateObject(Outer);
		ASSERT_THAT(IsNotNull(Payload));
		asVmAbortPartialConstruction(Payload, 1);
		ASSERT_THAT(AreEqual(1, GMemberDtor[0]));
		ASSERT_THAT(AreEqual(0, GOuterDtor));
		ASSERT_THAT(IsNull(asVmHeaderFromPayload(Payload)));
	}

	TEST_METHOD(ObjectSurvivesCallerSnapshotRelease)
	{
		ResetLifetimeTraces();
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Dtor = Fixture.DefineFunction(
			TEXT("Destruct"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttVoid), {}, Type);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddBehaviour(*Type, asBEHAVE_DESTRUCT, *Dtor)));
		asCDataType ReturnType = asCDataType::CreateType(Type, false);
		ReturnType.MakeReference(true);
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, ReturnType, {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(MoveTemp(Fixture.Image))));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Dtor->GetStableKey(), asFUNCTION(GenericLifeDestruct), asCALL_GENERIC)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT RunSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Type, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Run, RunSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded,
			Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 16, 0)));
		asSByteCodeOperand PtrLocal = ByteLocal(2);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PSF, MakeArrayView(&PtrLocal, 1))));
		asSByteCodeOperand AllocOps[2] = {ByteSlot(asEByteCodeOperandKind::TypeSlot, TypeSlot), ByteImm32(0)};
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_ALLOC, MakeArrayView(AllocOps, 2))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshVPtr, MakeArrayView(&PtrLocal, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PopRPtr, TConstArrayView<asSByteCodeOperand>())));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		asIScriptContext* Context = Engine.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
		void* Object = Context->GetReturnAddress();
		ASSERT_THAT(IsNotNull(Object));
		ASSERT_THAT(IsNotNull(asVmHeaderFromPayload(Object)));
		Snapshot->Release();
		Snapshot = nullptr;
		ASSERT_THAT(IsNotNull(asVmHeaderFromPayload(Object)));
		ASSERT_THAT(AreEqual(0, GLifeDtor));
		asVmRelease(Object);
		ASSERT_THAT(AreEqual(1, GLifeDtor));
		Context->Release();
	}
};

#endif
