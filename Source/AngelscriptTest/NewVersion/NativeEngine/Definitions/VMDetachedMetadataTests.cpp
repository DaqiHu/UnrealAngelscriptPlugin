#include "CQTest.h"
#include "NativeDetachedDefinitionTestSupport.h"
#include "as_metadata_fingerprint.h"
#include "as_scriptengine.h"
#include "Async/ParallelFor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMDetachedMetadata, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(HandBuiltCounterNeedsNoEngineOrNumericIds)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Counter = Fixture.DefineType(TEXT("Counter"), TEXT("Gameplay"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsNotNull(Counter));
		asCObjectProperty* Value = Fixture.DefineProperty(*Counter, "Value", Fixture.Image->Primitive(ttInt));
		ASSERT_THAT(IsNotNull(Value));
		asCScriptFunction* Read = Fixture.DefineFunction(
			TEXT("Read"), TEXT("Gameplay"), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {}, Counter);
		ASSERT_THAT(IsNotNull(Read));
		asSMetadataParameter Parameter;
		Parameter.Name = "Amount";
		Parameter.Type = Fixture.Image->Primitive(ttInt);
		Parameter.Type.MakeReference(true);
		Parameter.Flags = asTM_INREF;
		asCScriptFunction* Global = Fixture.DefineFunction(
			TEXT("Add"), TEXT("Gameplay"), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), MakeArrayView(&Parameter, 1));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded, Counter->TryGetSchemaHash(Schema)));
		ASSERT_THAT(IsFalse(Schema.Digest.IsZero()));
		ASSERT_THAT(IsTrue(Counter->GetMethodByIndex(0) == Read));
		ASSERT_THAT(IsTrue(Read->GetObjectType() == Counter));
		ASSERT_THAT(IsNull(Counter->GetEngine()));
		ASSERT_THAT(IsNull(Read->GetEngine()));
		ASSERT_THAT(IsTrue(Counter->GetTypeId() < 0));
		ASSERT_THAT(IsTrue(Read->GetId() <= 0));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay")), FString(ANSI_TO_TCHAR(Counter->GetNamespace()))));
	}

	TEST_METHOD(OrdinaryFunctionKeyCanAgreeWhenReturnTypeDiffers)
	{
		FDetachedDefinitionFixture IntReturn;
		IntReturn.Scope = TEXT("vm-detached.return");
		asCObjectType* LeftOwner = IntReturn.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Left = IntReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, IntReturn.Image->Primitive(ttInt), {}, LeftOwner);
		ASSERT_THAT(IsNotNull(Left));
		ASSERT_THAT(IsTrue(IntReturn.FinalizeAndFreeze()));

		FDetachedDefinitionFixture Int64Return;
		Int64Return.Scope = TEXT("vm-detached.return");
		asCObjectType* RightOwner = Int64Return.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Right = Int64Return.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, Int64Return.Image->Primitive(ttInt64), {}, RightOwner);
		ASSERT_THAT(IsNotNull(Right));
		ASSERT_THAT(IsTrue(Int64Return.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Left->GetStableKey() == Right->GetStableKey()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash LeftSchema;
		AS_NAMESPACE_QUALIFIER asSSchemaHash RightSchema;
		TConstArrayView<uint8> LeftWitness;
		TConstArrayView<uint8> RightWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			LeftOwner->TryGetSchemaHash(LeftSchema, &LeftWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			RightOwner->TryGetSchemaHash(RightSchema, &RightWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(LeftSchema, LeftWitness, RightSchema, RightWitness)));
	}

	TEST_METHOD(ShellFreezeStillRejectsFingerprints)
	{
		auto Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;
		uint8 Bytes[32] = {};
		Bytes[0] = 9;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Key)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Image->CreateObjectType(Key, "Shell", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Image->Freeze()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::UnauthenticatedDefinition, Type->TryGetSchemaHash(Schema)));
	}

	TEST_METHOD(FunctionLeaseRetainsOwnerAfterFixtureRelease)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Node"), TEXT("Graph"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Next"), TEXT("Graph"), asFUNC_SCRIPT, asCDataType::CreateObjectHandle(Type, false), {}, Type);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Method->GetObjectType() == Type));
		ASSERT_THAT(AreEqual(FString(TEXT("Graph")), FString(ANSI_TO_TCHAR(Type->GetNamespace()))));
		ASSERT_THAT(IsNotNull(Fixture.Image.Get()));
	}

	TEST_METHOD(ConcurrentLeaseReleaseFromHeldReference)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Held"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		std::atomic<int32> Extra{0};
		ParallelFor(8, [&](int32)
		{
			Type->AddRef();
			Extra.fetch_add(1);
			Type->Release();
		});
		ASSERT_THAT(AreEqual(8, Extra.load()));
		ASSERT_THAT(IsNotNull(Fixture.Image.Get()));
		ASSERT_THAT(AreEqual(FString(TEXT("Held")), FString(ANSI_TO_TCHAR(Type->GetName()))));
	}

	TEST_METHOD(InvalidSignatureAndFrozenMutationLeaveCountsUnchanged)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsNotNull(Type));
		const int32 TypeCount = Fixture.Image->GetTypes().Num();
		const int32 FunctionCount = Fixture.Image->GetFunctions().Num();
		asCScriptFunction* Rejected = Fixture.DefineFunction(
			TEXT("Bad"), TEXT(""), asFUNC_SCRIPT, asCDataType(), {}, Type);
		ASSERT_THAT(IsNull(Rejected));
		ASSERT_THAT(AreEqual(TypeCount, Fixture.Image->GetTypes().Num()));
		ASSERT_THAT(AreEqual(FunctionCount, Fixture.Image->GetFunctions().Num()));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCObjectProperty* Late = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidState,
			Fixture.Image->AddProperty(*Type, "Late", Fixture.Image->Primitive(ttInt), Late)));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Type->GetPropertyCount())));
	}
};

#endif
