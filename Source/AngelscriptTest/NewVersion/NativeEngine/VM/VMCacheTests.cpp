#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_bytecode_image_codec.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_objecttype.h"
#include "as_property.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GCacheAddCalls = 0;

	void ResetCacheAdd()
	{
		GCacheAddCalls = 0;
	}

	void GenericCacheAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GCacheAddCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	asSMetadataParameter MakeCacheIntParam(const char* Name)
	{
		asSMetadataParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = asCDataType::CreatePrimitive(ttInt, false);
		return Parameter;
	}

	TUniquePtr<asCByteCodeImage> MakeReturn42(asCScriptFunction* Function, asCTypeInfo* ExtraType = nullptr)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (ExtraType)
		{
			Builder.AddTypeRequirement(*ExtraType, Slot);
		}
		Builder.AddFunctionRequirement(*Function, Slot);
		Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0);
		asSByteCodeOperand Push = ByteImm32(42);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		return Image;
	}

	TUniquePtr<asCByteCodeImage> MakeNativeAddImage(asCScriptFunction* Run, asCScriptFunction* Add)
	{
		asCByteCodeImageBuilder Builder;
		asUINT AddSlot = 0;
		asUINT RunSlot = 0;
		Builder.AddFunctionRequirement(*Add, AddSlot);
		Builder.AddFunctionRequirement(*Run, RunSlot);
		Builder.BeginFunction(Run->GetStableKey(), Run->GetStableKey(), 8, 0);
		asSByteCodeOperand Right = ByteImm32(22);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Right, 1));
		asSByteCodeOperand Left = ByteImm32(20);
		Builder.Emit(asBC_PshC4, MakeArrayView(&Left, 1));
		asSByteCodeOperand Call = ByteSlot(asEByteCodeOperandKind::FunctionSlot, AddSlot);
		Builder.Emit(asBC_CALLSYS, MakeArrayView(&Call, 1));
		asSByteCodeOperand Ret = ByteImm16(0);
		Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1));
		Builder.EndFunction();
		TUniquePtr<asCByteCodeImage> Image;
		Builder.Finish(Image);
		return Image;
	}

	asCObjectType* DefinePoint(FDetachedDefinitionFixture& Fixture, eTokenType FieldToken = ttInt)
	{
		asCObjectType* Type = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		if (!Type) return nullptr;
		Fixture.DefineProperty(*Type, "x", Fixture.Image->Primitive(FieldToken));
		return Type;
	}
}

TEST_CLASS_WITH_FLAGS(VMCache, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(EncodedImageExecutesOnFreshEngineAfterProducerRelease)
	{
		TArray<uint8> Bytes;
		asSStableKey FunctionKey;
		{
			FDetachedDefinitionFixture Fixture;
			asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			FunctionKey = Function->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
			TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(Function)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		}
		FDetachedDefinitionFixture FixtureB;
		asCScriptFunction* FunctionB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(FunctionB->GetStableKey() == FunctionKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(ContextB->GetReturnDWord())));
		ContextB->Release();
		SnapshotB->Release();
	}

	TEST_METHOD(TwoLiveEnginesHaveIndependentSnapshots)
	{
		FDetachedDefinitionFixture FixtureA;
		asCScriptFunction* FunctionA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		FDetachedDefinitionFixture FixtureB;
		asCScriptFunction* FunctionB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(FunctionA);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		FNativeEngineOwner EngineA;
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(FixtureA.Image)));
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		asCExecutableSnapshot* SnapshotA = nullptr;
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->LinkByteCodeImage(*Decoded, SnapshotA)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		ASSERT_THAT(IsTrue(SnapshotA != SnapshotB));
		asIScriptContext* ContextA = EngineA.Ptr->CreateContext();
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextA->Prepare(FunctionA)));
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(FunctionB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextA->Execute()));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(ContextA->GetReturnDWord())));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(ContextB->GetReturnDWord())));
		ContextA->Release();
		ContextB->Release();
		SnapshotA->Release();
		SnapshotB->Release();
	}

	TEST_METHOD(NativeAddPortableAcrossFreshEngine)
	{
		ResetCacheAdd();
		TArray<uint8> Bytes;
		asSStableKey RunKey;
		asSStableKey AddKey;
		{
			FDetachedDefinitionFixture Fixture;
			asSMetadataParameter Params[2] = {MakeCacheIntParam("a"), MakeCacheIntParam("b")};
			Params[0].Type = Fixture.Image->Primitive(ttInt);
			Params[1].Type = Fixture.Image->Primitive(ttInt);
			asCScriptFunction* Add = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
			asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
			ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
			RunKey = Run->GetStableKey();
			AddKey = Add->GetStableKey();
			FNativeEngineOwner EngineA;
			ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineA.Ptr->RegisterMetadataImage(Fixture.Image)));
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
				EngineA.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(GenericCacheAdd), asCALL_GENERIC)));
			TUniquePtr<asCByteCodeImage> Image = MakeNativeAddImage(Run, Add);
			asCExecutableSnapshot* Snapshot = nullptr;
			ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineA.Ptr->LinkByteCodeImage(*Image, Snapshot)));
			asIScriptContext* Context = EngineA.Ptr->CreateContext();
			ASSERT_THAT(AreEqual(0, Context->Prepare(Run)));
			ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Context->Execute()));
			ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord())));
			Context->Release();
			Snapshot->Release();
			ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		}
		ResetCacheAdd();
		FDetachedDefinitionFixture FixtureB;
		asSMetadataParameter ParamsB[2] = {MakeCacheIntParam("a"), MakeCacheIntParam("b")};
		ParamsB[0].Type = FixtureB.Image->Primitive(ttInt);
		ParamsB[1].Type = FixtureB.Image->Primitive(ttInt);
		asCScriptFunction* AddB = FixtureB.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, FixtureB.Image->Primitive(ttInt), MakeArrayView(ParamsB, 2));
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(RunB->GetStableKey() == RunKey));
		ASSERT_THAT(IsTrue(AddB->GetStableKey() == AddKey));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			EngineB.Ptr->BindNativeFunction(AddB->GetStableKey(), asFUNCTION(GenericCacheAdd), asCALL_GENERIC)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* SnapshotB = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, EngineB.Ptr->LinkByteCodeImage(*Decoded, SnapshotB)));
		asIScriptContext* ContextB = EngineB.Ptr->CreateContext();
		ASSERT_THAT(AreEqual(0, ContextB->Prepare(RunB)));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ContextB->Execute()));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(ContextB->GetReturnDWord())));
		ASSERT_THAT(AreEqual(1, GCacheAddCalls));
		ContextB->Release();
		SnapshotB->Release();
	}

	TEST_METHOD(MissingDestinationFunctionRejectsWithoutMutation)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
		FNativeEngineOwner Engine;
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(MissingNativeBindingOnDestinationRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asSMetadataParameter Params[2] = {MakeCacheIntParam("a"), MakeCacheIntParam("b")};
		Params[0].Type = Fixture.Image->Primitive(ttInt);
		Params[1].Type = Fixture.Image->Primitive(ttInt);
		asCScriptFunction* Add = Fixture.DefineFunction(TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Image->Primitive(ttInt), MakeArrayView(Params, 2));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeNativeAddImage(Run, Add);
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->RegisterMetadataImage(Fixture.Image)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, Engine.Ptr->LinkByteCodeImage(*Image, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
		ASSERT_THAT(AreEqual(0, GCacheAddCalls));
	}

	TEST_METHOD(DestinationSchemaChangeRejectsLink)
	{
		FDetachedDefinitionFixture FixtureA;
		asCObjectType* PointA = DefinePoint(FixtureA, ttInt);
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(RunA, PointA);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture FixtureB;
		DefinePoint(FixtureB, ttInt64);
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, EngineB.Ptr->LinkByteCodeImage(*Decoded, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(IndirectReferencedTypeMismatchRejects)
	{
		FDetachedDefinitionFixture FixtureA;
		asCObjectType* ItemA = FixtureA.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		FixtureA.DefineProperty(*ItemA, "value", FixtureA.Image->Primitive(ttInt));
		asCObjectType* OwnerA = FixtureA.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCDataType Handle = asCDataType::CreateType(ItemA, false);
		ASSERT_THAT(IsTrue(Handle.MakeHandle(true, true) >= 0));
		FixtureA.DefineProperty(*OwnerA, "item", Handle);
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(RunA, OwnerA);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture FixtureB;
		asCObjectType* ItemB = FixtureB.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		FixtureB.DefineProperty(*ItemB, "value", FixtureB.Image->Primitive(ttInt64));
		asCObjectType* OwnerB = FixtureB.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCDataType HandleB = asCDataType::CreateType(ItemB, false);
		ASSERT_THAT(IsTrue(HandleB.MakeHandle(true, true) >= 0));
		FixtureB.DefineProperty(*OwnerB, "item", HandleB);
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, EngineB.Ptr->LinkByteCodeImage(*Decoded, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}

	TEST_METHOD(TamperedBytesRejectDecode)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(Function);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		ASSERT_THAT(IsTrue(Bytes.Num() > 8));
		Bytes[0] = 1;
		TUniquePtr<asCByteCodeImage> Decoded;
		const asEByteCodeImageStatus Status = asCByteCodeImageCodec::Decode(Bytes, Decoded);
		ASSERT_THAT(IsTrue(Status != asEByteCodeImageStatus::Succeeded));
		ASSERT_THAT(IsNull(Decoded.Get()));
	}

	TEST_METHOD(DestinationLayoutChangeRejectsLink)
	{
		FDetachedDefinitionFixture FixtureA;
		asCObjectType* PointA = DefinePoint(FixtureA, ttInt);
		FixtureA.DefineProperty(*PointA, "y", FixtureA.Image->Primitive(ttInt));
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeReturn42(RunA, PointA);
		TArray<uint8> Bytes;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Encode(*Image, Bytes)));
		FDetachedDefinitionFixture FixtureB;
		asCObjectType* PointB = DefinePoint(FixtureB, ttInt);
		FixtureB.DefineProperty(*PointB, "z", FixtureB.Image->Primitive(ttInt));
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, EngineB.Ptr->RegisterMetadataImage(FixtureB.Image)));
		TUniquePtr<asCByteCodeImage> Decoded;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, asCByteCodeImageCodec::Decode(Bytes, Decoded)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, EngineB.Ptr->LinkByteCodeImage(*Decoded, Snapshot)));
		ASSERT_THAT(IsNull(Snapshot));
	}
};

#endif
