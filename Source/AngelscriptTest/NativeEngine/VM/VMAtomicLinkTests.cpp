#include "CQTest.h"
#include "NativeVMTestSupport.h"
#include "as_datatype.h"
#include "as_generic.h"
#include "as_metadata_fingerprint.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "frontend/Sema/as_type_identity.h"

#include <atomic>
#include <thread>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 GAtomicNativeCalls = 0;

	struct FAtomicZeroDigest final : AS_NAMESPACE_QUALIFIER asIFingerprintDigestProvider
	{
		AS_NAMESPACE_QUALIFIER asSFingerprintDigest ComputeDigest(TConstArrayView<uint8>) const override
		{
			return {};
		}
	};

	void AtomicGenericAdd(asIScriptGeneric* Generic)
	{
		const int32 A = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 B = static_cast<int32>(Generic->GetArgDWord(1));
		++GAtomicNativeCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(A + B));
	}

	asSDefinitionParameter MakeAtomicIntParam(const char* Name)
	{
		asSDefinitionParameter Param;
		Param.Name = Name;
		return Param;
	}

	asSByteCodeRequirement* FindAtomicRequirement(
		asCByteCodeImage& Image,
		asEByteCodeRequirementRole Role,
		const asSStableKey& Key)
	{
		for (asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Role == Role && Requirement.Key == Key)
			{
				return &Requirement;
			}
		}
		return nullptr;
	}

	asSByteCodeRequirement* FindAtomicRole(asCByteCodeImage& Image, asEByteCodeRequirementRole Role)
	{
		for (asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Role == Role)
			{
				return &Requirement;
			}
		}
		return nullptr;
	}

	TUniquePtr<asCByteCodeImage> MakeAtomicReturn(
		asCScriptFunction* Function,
		int32 Value,
		asCTypeInfo* ExtraType = nullptr)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (ExtraType)
		{
			if (Builder.AddTypeRequirement(*ExtraType, Slot) != asEByteCodeImageStatus::Succeeded)
			{
				return nullptr;
			}
		}
		if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Push = ByteImm32(Value);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}

	bool ExecutePreparedInt(asCScriptEngine* Engine, asCScriptFunction* Function, int32 Expected)
	{
		asIScriptContext* Context = Engine->CreateContext();
		if (!Context)
		{
			return false;
		}
		const bool bOk = Context->Prepare(Function) == 0
			&& Context->Execute() == asEXECUTION_FINISHED
			&& static_cast<int32>(Context->GetReturnDWord()) == Expected;
		Context->Release();
		return bOk;
	}

	asCGlobalProperty* DefineAtomicGlobal(FDetachedDefinitionFixture& Fixture, const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Declaration;
		Declaration.StableScope = Fixture.Scope;
		Declaration.Name = Name;
		Declaration.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
		asSStableKey Key;
		if (Fixture.Context->GetIdentityRegistry().InternDeclaration(Declaration, Key)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TypeUse;
		TypeUse.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		TypeUse.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32;
		TypeUse.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
		asSStableKey TypeKey;
		if (Fixture.Context->GetIdentityRegistry().InternTypeUse(TypeUse, TypeKey)
			!= AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCGlobalProperty* Global = nullptr;
		if (Fixture.Set->CreateGlobalProperty(
			Key, TypeKey, TCHAR_TO_UTF8(Name), "", Fixture.Set->Primitive(ttInt, true), Global)
			!= asEDefinitionResult::Succeeded)
		{
			return nullptr;
		}
		return Global;
	}

	asSStableKey MakeAtomicMissingKey(uint8 Tag)
	{
		uint8 Bytes[32] = {};
		Bytes[0] = Tag;
		asSStableKey Key;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
		return Key;
	}

	TUniquePtr<asCByteCodeImage> MakeAtomicReturnWithMissingSibling(asCScriptFunction* Function, int32 Value)
	{
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		if (Builder.AddFunctionRequirement(*Function, Slot) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Push = ByteImm32(Value);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand Ret = ByteImm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		const asSStableKey Missing = MakeAtomicMissingKey(0x5A);
		if (Builder.BeginFunction(Missing, Missing, 4, 0) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		asSByteCodeOperand PushG = ByteImm32(7);
		if (Builder.Emit(asBC_PshC4, MakeArrayView(&PushG, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		TUniquePtr<asCByteCodeImage> Image;
		if (Builder.Finish(Image) != asEByteCodeImageStatus::Succeeded)
		{
			return nullptr;
		}
		return Image;
	}
}

TEST_CLASS_WITH_FLAGS(VMAtomicLink, "Angelscript.UnitTest.NativeEngine.VM",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(MissingSiblingLeavesFUnpublishedThenRetrySucceeds)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		const auto Counts = Engine.Ptr->GetRegistrationCounts();
		TUniquePtr<asCByteCodeImage> Partial = MakeAtomicReturnWithMissingSibling(Function, 41);
		ASSERT_THAT(IsNotNull(Partial.Get()));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*Engine.Ptr, *Partial, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Counts));
		TUniquePtr<asCByteCodeImage> Good = MakeAtomicReturn(Function, 42);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Good, Snapshot)));
		ASSERT_THAT(IsNotNull(Snapshot));
		ASSERT_THAT(IsNotNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		ASSERT_THAT(IsTrue(ExecutePreparedInt(Engine.Ptr, Function, 42)));
		Snapshot->Release();
	}

	TEST_METHOD(UnboundNativeThenRetryAfterBind)
	{
		GAtomicNativeCalls = 0;
		FDetachedDefinitionFixture Fixture;
		asSDefinitionParameter Params[2] = { MakeAtomicIntParam("a"), MakeAtomicIntParam("b") };
		Params[0].Type = Fixture.Set->Primitive(ttInt);
		Params[1].Type = Fixture.Set->Primitive(ttInt);
		asCScriptFunction* Add = Fixture.DefineFunction(
			TEXT("Add"), TEXT(""), asFUNC_SYSTEM, Fixture.Set->Primitive(ttInt), MakeArrayView(Params, 2));
		asCScriptFunction* Run = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
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
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingBinding, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Run->GetStableKey())));
		ASSERT_THAT(AreEqual(0, GAtomicNativeCalls));
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded,
			Engine.Ptr->BindNativeFunction(Add->GetStableKey(), asFUNCTION(AtomicGenericAdd), asCALL_GENERIC)));
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot)));
		ASSERT_THAT(IsTrue(ExecutePreparedInt(Engine.Ptr, Run, 42)));
		ASSERT_THAT(AreEqual(1, GAtomicNativeCalls));
		ASSERT_THAT(IsNull(Add->sysFuncIntf));
		Snapshot->Release();
	}

	TEST_METHOD(ZeroDigestRequirementRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42, Point);
		ASSERT_THAT(IsNotNull(Image.Get()));
		asSByteCodeRequirement* TypeReq = FindAtomicRequirement(*Image, asEByteCodeRequirementRole::Type, Point->GetStableKey());
		ASSERT_THAT(IsNotNull(TypeReq));
		TypeReq->Schema = {};
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
	}

	TEST_METHOD(FailedFingerprintQueryRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FAtomicZeroDigest Provider;
		FNativeEngineOwner Engine;
		asCDefinitions* Installed = Fixture.Set.Get();
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42, Point);
		Installed->SetFingerprintDigestProvider(&Provider);
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		Installed->SetFingerprintDigestProvider(nullptr);
	}

	TEST_METHOD(WitnessCollisionRejectsDigestMatch)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42, Point);
		asSByteCodeRequirement* TypeReq = FindAtomicRequirement(*Image, asEByteCodeRequirementRole::Type, Point->GetStableKey());
		ASSERT_THAT(IsNotNull(TypeReq));
		ASSERT_THAT(IsTrue(TypeReq->SchemaWitness.Num() > 0));
		TypeReq->SchemaWitness[0] ^= 0x5A;
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
	}

	TEST_METHOD(ReturnOnlyChangeRejects)
	{
		FDetachedDefinitionFixture IntReturn;
		IntReturn.Scope = TEXT("vm-atomic-return");
		asCScriptFunction* Left = IntReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, IntReturn.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(IntReturn.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Left, 42);
		FDetachedDefinitionFixture DoubleReturn;
		DoubleReturn.Scope = TEXT("vm-atomic-return");
		asCScriptFunction* Right = DoubleReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, DoubleReturn.Set->Primitive(ttFloat64), {});
		ASSERT_THAT(IsTrue(DoubleReturn.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Left->GetStableKey() == Right->GetStableKey()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, Engine.Ptr->RegisterExternalDefinitions(DoubleReturn.TakeSet())));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Right->GetStableKey())));
	}

	TEST_METHOD(OffsetOnlyPropertyChangeRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* Field = Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT PropSlot = 0;
		asUINT FnSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Point, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddPropertyRequirement(*Point, *Field, PropSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, FnSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Function->GetStableKey(), Function->GetStableKey(), 4, 0)));
		asSByteCodeOperand Push = ByteImm32(42);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Image)));
		asSByteCodeRequirement* PropReq = FindAtomicRole(*Image, asEByteCodeRequirementRole::Property);
		ASSERT_THAT(IsNotNull(PropReq));
		PropReq->NativeOffset += 4;
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
	}

	TEST_METHOD(PointeeOnlyChangeRejects)
	{
		FDetachedDefinitionFixture FixtureA;
		asCObjectType* ItemA = FixtureA.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		FixtureA.DefineProperty(*ItemA, "value", FixtureA.Set->Primitive(ttInt));
		asCObjectType* OwnerA = FixtureA.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCDataType Handle = asCDataType::CreateType(ItemA, false);
		ASSERT_THAT(IsTrue(Handle.MakeHandle(true, true) >= 0));
		FixtureA.DefineProperty(*OwnerA, "item", Handle);
		asCScriptFunction* RunA = FixtureA.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureA.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureA.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(RunA, 42, OwnerA);
		FDetachedDefinitionFixture FixtureB;
		asCObjectType* ItemB = FixtureB.DefineType(TEXT("Item"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		FixtureB.DefineProperty(*ItemB, "value", FixtureB.Set->Primitive(ttInt64));
		asCObjectType* OwnerB = FixtureB.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCDataType HandleB = asCDataType::CreateType(ItemB, false);
		ASSERT_THAT(IsTrue(HandleB.MakeHandle(true, true) >= 0));
		FixtureB.DefineProperty(*OwnerB, "item", HandleB);
		asCScriptFunction* RunB = FixtureB.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, FixtureB.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(FixtureB.FinalizeAndFreeze()));
		FNativeEngineOwner EngineB;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, EngineB.Ptr->RegisterExternalDefinitions(FixtureB.TakeSet())));
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*EngineB.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(EngineB.Ptr->FindPublishedExecutable(RunB->GetStableKey())));
	}

	TEST_METHOD(RetiredAndForeignReject)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42);
		FNativeEngineOwner Foreign;
		asCExecutableSnapshot* Missing = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::MissingDeclaration, asLinkByteCodeImage(*Foreign.Ptr, *Image, Missing)));
		ASSERT_THAT(IsNull(Missing));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		Engine.Ptr->RetireExternalDefinitions();
		asCExecutableSnapshot* Retired = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Retired, asLinkByteCodeImage(*Engine.Ptr, *Image, Retired)));
		ASSERT_THAT(IsNull(Retired));
	}

	TEST_METHOD(FrozenTamperedMetadataRejects)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42, Point);
		asSSchemaHash Schema;
		TConstArrayView<uint8> Witness;
		ASSERT_THAT(AreEqual(asEFingerprintStatus::Succeeded, Point->TryGetSchemaHash(Schema, &Witness)));
		ASSERT_THAT(IsTrue(Point->localProperties.GetLength() > 0));
		Point->localProperties[0]->name = "tampered";
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::SchemaMismatch, asLinkByteCodeImage(*Engine.Ptr, *Image, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		Point->localProperties[0]->name = "x";
	}

	TEST_METHOD(FailedCandidateRestoresCountsAndSentinels)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Point = Fixture.DefineType(TEXT("Point"), TEXT(""), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT);
		Fixture.DefineProperty(*Point, "x", Fixture.Set->Primitive(ttInt));
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		asCGlobalProperty* Global = DefineAtomicGlobal(Fixture, TEXT("Answer"));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		int32 Sentinel = 0x11223344;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, Engine.Ptr->BindGlobalStorage(Global->GetStableKey(), &Sentinel)));
		asCByteCodeImageBuilder Builder;
		asUINT TypeSlot = 0;
		asUINT FnSlot = 0;
		asUINT GlobalSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Point, TypeSlot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, FnSlot)));
		asSByteCodeRequirement GlobalReq;
		GlobalReq.Role = asEByteCodeRequirementRole::Global;
		GlobalReq.Key = Global->GetStableKey();
		GlobalReq.Name = TEXT("Answer");
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(GlobalReq, GlobalSlot)));
		const asSStableKey Missing = MakeAtomicMissingKey(0x77);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.BeginFunction(Missing, Missing, 4, 0)));
		asSByteCodeOperand Push = ByteImm32(1);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_PshC4, MakeArrayView(&Push, 1))));
		asSByteCodeOperand Ret = ByteImm16(0);
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1))));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.EndFunction()));
		TUniquePtr<asCByteCodeImage> Bad;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.Finish(Bad)));
		const auto Counts = Engine.Ptr->GetRegistrationCounts();
		const int32 FunctionRefs = Function->externalRefCount.get();
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Bad, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsTrue(Engine.Ptr->GetRegistrationCounts() == Counts));
		ASSERT_THAT(AreEqual(FunctionRefs, Function->externalRefCount.get()));
		ASSERT_THAT(AreEqual(0x11223344, Sentinel));
		ASSERT_THAT(IsTrue(Engine.Ptr->FindBoundGlobalStorage(Global->GetStableKey()) == &Sentinel));
		ASSERT_THAT(IsNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
	}

	TEST_METHOD(InstalledBodyRemainsCallableAfterFailedCandidate)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Good = MakeAtomicReturn(Function, 42);
		asCExecutableSnapshot* Snapshot = nullptr;
		ASSERT_THAT(AreEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Good, Snapshot)));
		TUniquePtr<asCByteCodeImage> Bad = MakeAtomicReturnWithMissingSibling(Function, 1);
		asCExecutableSnapshot* Failed = nullptr;
		ASSERT_THAT(AreNotEqual(asEByteCodeLinkStatus::Succeeded, asLinkByteCodeImage(*Engine.Ptr, *Bad, Failed)));
		ASSERT_THAT(IsNull(Failed));
		ASSERT_THAT(IsNotNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		ASSERT_THAT(IsTrue(ExecutePreparedInt(Engine.Ptr, Function, 42)));
		ASSERT_THAT(IsTrue(Function->scriptData == nullptr || Function->scriptData->byteCode.GetLength() == 0));
		Snapshot->Release();
	}

	TEST_METHOD(TwoThreadsOnePublicationWinner)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Set->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		FNativeEngineOwner Engine;
		ASSERT_THAT(AreEqual(asERegistrationResult::Succeeded, RegisterCompiledDefinitions(*Engine.Ptr, MoveTemp(Fixture.Set))));
		TUniquePtr<asCByteCodeImage> Image = MakeAtomicReturn(Function, 42);
		std::atomic<int32> Ready{0};
		std::atomic<int32> Successes{0};
		std::atomic<int32> Conflicts{0};
		std::atomic<int32> Other{0};
		std::atomic<asCExecutableSnapshot*> Winner{nullptr};
		auto Worker = [&]()
		{
			Ready.fetch_add(1);
			while (Ready.load() < 2)
			{
			}
			asCExecutableSnapshot* Snapshot = nullptr;
			const asEByteCodeLinkStatus Status = asLinkByteCodeImage(*Engine.Ptr, *Image, Snapshot);
			if (Status == asEByteCodeLinkStatus::Succeeded)
			{
				Successes.fetch_add(1);
				asCExecutableSnapshot* Previous = Winner.exchange(Snapshot);
				if (Previous && Previous != Snapshot)
				{
					Previous->Release();
				}
			}
			else if (Status == asEByteCodeLinkStatus::BodyConflict)
			{
				Conflicts.fetch_add(1);
				if (Snapshot)
				{
					Snapshot->Release();
				}
			}
			else
			{
				Other.fetch_add(1);
				if (Snapshot)
				{
					Snapshot->Release();
				}
			}
		};
		std::thread First(Worker);
		std::thread Second(Worker);
		First.join();
		Second.join();
		ASSERT_THAT(AreEqual(1, Successes.load()));
		ASSERT_THAT(AreEqual(1, Conflicts.load()));
		ASSERT_THAT(AreEqual(0, Other.load()));
		ASSERT_THAT(IsNotNull(Engine.Ptr->FindPublishedExecutable(Function->GetStableKey())));
		ASSERT_THAT(IsTrue(ExecutePreparedInt(Engine.Ptr, Function, 42)));
		if (asCExecutableSnapshot* Held = Winner.load())
		{
			Held->Release();
		}
	}
};

#endif
