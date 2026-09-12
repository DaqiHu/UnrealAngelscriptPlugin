#include "CQTest.h"
#include "as_metadata_fingerprint.h"
#include "as_module_definition_set.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "frontend/as_canonical_encoding.h"
#include "frontend/as_type_context.h"
#include "Async/ParallelFor.h"
#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMFingerprints, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCollidingDigest final : AS_NAMESPACE_QUALIFIER asIFingerprintDigestProvider
	{
		AS_NAMESPACE_QUALIFIER asSFingerprintDigest ComputeDigest(TConstArrayView<uint8>) const override
		{
			uint8 Bytes[32];
			FMemory::Memset(Bytes, 0x11, 32);
			AS_NAMESPACE_QUALIFIER asSFingerprintDigest Digest;
			AS_NAMESPACE_QUALIFIER asSFingerprintDigest::TryCreate(MakeArrayView(Bytes), Digest);
			return Digest;
		}
	};

	struct FEngineOwner
	{
		asCScriptEngine* Ptr = static_cast<asCScriptEngine*>(asCreateScriptEngine());
		~FEngineOwner() { if (Ptr) Ptr->ShutDownAndRelease(); }
		void Reset() { if (Ptr) Ptr->ShutDownAndRelease(); Ptr = nullptr; }
	};

	struct FPointSet
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
		asCObjectType* Type = nullptr;
		asCObjectProperty* X = nullptr;
		asCObjectProperty* Y = nullptr;
		asCScriptFunction* Method = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey Int32Use;
		AS_NAMESPACE_QUALIFIER asSStableKey Int64Use;

		bool InternPrimitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive, AS_NAMESPACE_QUALIFIER asSStableKey& Out)
		{
			return Context->InternPrimitiveTypeUse(Primitive, Out) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded;
		}

		bool CreateType(const TCHAR* Name, const TCHAR* Scope, asQWORD Flags, asCObjectType*& Out)
		{
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = Scope;
			Descriptor.Name = Name;
			Descriptor.Kind = (Flags & asOBJ_VALUE)
				? AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct
				: AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			return Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
				&& Set->CreateObjectType(Key, TCHAR_TO_UTF8(Name), "", Flags, Out) == asEMetadataResult::Succeeded;
		}

		bool AddIntField(asCObjectType& Owner, const char* Name, eTokenType Token, asCObjectProperty*& Out)
		{
			return Set->AddProperty(Owner, Name, Set->Primitive(Token), Out) == asEMetadataResult::Succeeded;
		}

		bool AddMethod(asCObjectType& Owner, const char* Name, eTokenType ReturnToken, asCScriptFunction*& Out)
		{
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
			Descriptor.StableScope = TEXT("vm-fingerprints");
			Descriptor.Name = UTF8_TO_TCHAR(Name);
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			Descriptor.Owner = Owner.GetStableKey();
			if (ReturnToken == ttInt64)
			{
				if (!InternPrimitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int64, Descriptor.ReturnType)) return false;
			}
			else if (!InternPrimitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Descriptor.ReturnType))
			{
				return false;
			}
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			return Context->GetIdentityRegistry().InternFunction(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
				&& Set->CreateFunction(Key, Name, "", asFUNC_SCRIPT, Out) == asEMetadataResult::Succeeded
				&& Set->DefineFunction(*Out, Set->Primitive(ReturnToken), {}, &Owner) == asEMetadataResult::Succeeded
				&& Set->SetFunctionDeclarationFacts(*Out, AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method, &Owner) == asEMetadataResult::Succeeded;
		}

		bool BuildPoint(bool bUnrelatedFirst = false, eTokenType XType = ttInt, const char* YName = "y", bool bReverseFields = false)
		{
			if (Set->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			if (!InternPrimitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Int32Use)) return false;
			if (!InternPrimitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int64, Int64Use)) return false;
			if (bUnrelatedFirst)
			{
				asCObjectType* Unrelated = nullptr;
				if (!CreateType(TEXT("Unrelated"), TEXT("vm-fingerprints.unrelated"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Unrelated))
				{
					return false;
				}
			}
			if (!CreateType(TEXT("Point"), TEXT("vm-fingerprints"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type)) return false;
			if (bReverseFields)
			{
				if (!AddIntField(*Type, YName, ttInt, Y) || !AddIntField(*Type, "x", XType, X)) return false;
			}
			else
			{
				if (!AddIntField(*Type, "x", XType, X) || !AddIntField(*Type, YName, ttInt, Y)) return false;
			}
			return Set->FinalizeLayouts() == asEMetadataResult::Succeeded
				&& Set->Freeze() == asEMetadataResult::Succeeded
				&& Set->ValidateFrozenDefinitions();
		}
	};

	static void WriteKey(AS_NAMESPACE_QUALIFIER asCCanonicalEncoding& Encoding, const AS_NAMESPACE_QUALIFIER asSStableKey& Key)
	{
		Encoding.WriteBytes(Key.GetBytes());
	}

	static TArray<uint8> IndependentPointSchema(const AS_NAMESPACE_QUALIFIER asSStableKey& Int32Use)
	{
		AS_NAMESPACE_QUALIFIER asCCanonicalEncoding Encoding(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Schema);
		Encoding.WriteUInt8(static_cast<uint8>(AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct));
		Encoding.WriteUInt32(5);
		WriteKey(Encoding, {});
		WriteKey(Encoding, {});
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(2);
		Encoding.WriteString(TEXT("x"));
		WriteKey(Encoding, Int32Use);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt8(0);
		Encoding.WriteString(TEXT(""));
		Encoding.WriteString(TEXT("y"));
		WriteKey(Encoding, Int32Use);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt8(0);
		Encoding.WriteString(TEXT(""));
		Encoding.WriteUInt32(0);
		WriteKey(Encoding, {});
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt32(0);
		TArray<uint8> Bytes;
		Bytes.Append(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
		return Bytes;
	}

	static TArray<uint8> IndependentPointLayout()
	{
		AS_NAMESPACE_QUALIFIER asCCanonicalEncoding Encoding(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Layout);
		Encoding.WriteUInt16(1);
		Encoding.WriteUInt8(static_cast<uint8>(sizeof(void*)));
		Encoding.WriteUInt8(1);
		Encoding.WriteUInt8(1);
		Encoding.WriteUInt8(1);
		Encoding.WriteUInt32(8);
		Encoding.WriteUInt32(4);
		WriteKey(Encoding, {});
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(2);
		Encoding.WriteString(TEXT("x"));
		Encoding.WriteUInt32(0);
		Encoding.WriteUInt32(4);
		Encoding.WriteUInt32(4);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt32(0);
		Encoding.WriteString(TEXT("y"));
		Encoding.WriteUInt32(4);
		Encoding.WriteUInt32(4);
		Encoding.WriteUInt32(4);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt8(0);
		Encoding.WriteUInt32(0);
		TArray<uint8> Bytes;
		Bytes.Append(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
		return Bytes;
	}

	static AS_NAMESPACE_QUALIFIER asSFingerprintDigest IndependentDigest(TConstArrayView<uint8> Bytes)
	{
		const FBlake3Hash Hash = FBlake3::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
		AS_NAMESPACE_QUALIFIER asSFingerprintDigest Digest;
		AS_NAMESPACE_QUALIFIER asSFingerprintDigest::TryCreate(
			MakeArrayView(Hash.GetBytes(), AS_NAMESPACE_QUALIFIER asSFingerprintDigest::ByteCount), Digest);
		return Digest;
	}

	static bool SameBytes(TConstArrayView<uint8> Left, TConstArrayView<uint8> Right)
	{
		return Left.Num() == Right.Num()
			&& (Left.Num() == 0 || FMemory::Memcmp(Left.GetData(), Right.GetData(), Left.Num()) == 0);
	}

public:
	TEST_METHOD(IndependentPointsAgreeOnKeyWitnessAndHash)
	{
		FPointSet First;
		FPointSet Second;
		ASSERT_THAT(IsTrue(First.BuildPoint(true)));
		ASSERT_THAT(IsTrue(Second.BuildPoint(false)));
		ASSERT_THAT(IsTrue(First.Type->GetStableKey() == Second.Type->GetStableKey()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash FirstSchema;
		AS_NAMESPACE_QUALIFIER asSSchemaHash SecondSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash FirstLayout;
		AS_NAMESPACE_QUALIFIER asSLayoutHash SecondLayout;
		TConstArrayView<uint8> FirstSchemaWitness;
		TConstArrayView<uint8> SecondSchemaWitness;
		TConstArrayView<uint8> FirstLayoutWitness;
		TConstArrayView<uint8> SecondLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			First.Type->TryGetSchemaHash(FirstSchema, &FirstSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Second.Type->TryGetSchemaHash(SecondSchema, &SecondSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			First.Type->TryGetLayoutHash(FirstLayout, &FirstLayoutWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Second.Type->TryGetLayoutHash(SecondLayout, &SecondLayoutWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Equal,
			CompareSchemaFingerprints(FirstSchema, FirstSchemaWitness, SecondSchema, SecondSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Equal,
			CompareLayoutFingerprints(FirstLayout, FirstLayoutWitness, SecondLayout, SecondLayoutWitness)));
	}

	TEST_METHOD(GoldenCanonicalBytesFixFieldOrderAndDomains)
	{
		FPointSet Point;
		ASSERT_THAT(IsTrue(Point.BuildPoint()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash Layout;
		TConstArrayView<uint8> SchemaWitness;
		TConstArrayView<uint8> LayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetSchemaHash(Schema, &SchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetLayoutHash(Layout, &LayoutWitness)));
		const TArray<uint8> ExpectedSchema = IndependentPointSchema(Point.Int32Use);
		const TArray<uint8> ExpectedLayout = IndependentPointLayout();
		ASSERT_THAT(IsTrue(SameBytes(SchemaWitness, ExpectedSchema)));
		ASSERT_THAT(IsTrue(SameBytes(LayoutWitness, ExpectedLayout)));
		ASSERT_THAT(IsTrue(Schema.Digest == IndependentDigest(ExpectedSchema)));
		ASSERT_THAT(IsTrue(Layout.Digest == IndependentDigest(ExpectedLayout)));
		ASSERT_THAT(AreEqual(static_cast<uint8>(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Schema), SchemaWitness[7]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Layout), LayoutWitness[7]));
	}

	TEST_METHOD(OneFactChangesAffectDeclaredDomains)
	{
		FPointSet Baseline;
		ASSERT_THAT(IsTrue(Baseline.BuildPoint()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash BaselineSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash BaselineLayout;
		TConstArrayView<uint8> BaselineSchemaWitness;
		TConstArrayView<uint8> BaselineLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Baseline.Type->TryGetSchemaHash(BaselineSchema, &BaselineSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Baseline.Type->TryGetLayoutHash(BaselineLayout, &BaselineLayoutWitness)));

		FPointSet WiderX;
		ASSERT_THAT(IsTrue(WiderX.BuildPoint(false, ttInt64)));
		AS_NAMESPACE_QUALIFIER asSSchemaHash WiderSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash WiderLayout;
		TConstArrayView<uint8> WiderSchemaWitness;
		TConstArrayView<uint8> WiderLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			WiderX.Type->TryGetSchemaHash(WiderSchema, &WiderSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			WiderX.Type->TryGetLayoutHash(WiderLayout, &WiderLayoutWitness)));
		ASSERT_THAT(IsTrue(WiderX.Type->GetStableKey() == Baseline.Type->GetStableKey()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(BaselineSchema, BaselineSchemaWitness, WiderSchema, WiderSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareLayoutFingerprints(BaselineLayout, BaselineLayoutWitness, WiderLayout, WiderLayoutWitness)));

		FPointSet Renamed;
		ASSERT_THAT(IsTrue(Renamed.BuildPoint(false, ttInt, "z")));
		AS_NAMESPACE_QUALIFIER asSSchemaHash RenamedSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash RenamedLayout;
		TConstArrayView<uint8> RenamedSchemaWitness;
		TConstArrayView<uint8> RenamedLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Renamed.Type->TryGetSchemaHash(RenamedSchema, &RenamedSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Renamed.Type->TryGetLayoutHash(RenamedLayout, &RenamedLayoutWitness)));
		ASSERT_THAT(IsTrue(Renamed.Type->GetStableKey() == Baseline.Type->GetStableKey()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(BaselineSchema, BaselineSchemaWitness, RenamedSchema, RenamedSchemaWitness)));

		FPointSet Reversed;
		ASSERT_THAT(IsTrue(Reversed.BuildPoint(false, ttInt, "y", true)));
		AS_NAMESPACE_QUALIFIER asSSchemaHash ReversedSchema;
		TConstArrayView<uint8> ReversedSchemaWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Reversed.Type->TryGetSchemaHash(ReversedSchema, &ReversedSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(BaselineSchema, BaselineSchemaWitness, ReversedSchema, ReversedSchemaWitness)));
	}

	TEST_METHOD(MethodReturnChangesSchemaButNotIdentity)
	{
		FPointSet IntReturn;
		ASSERT_THAT(IsTrue(IntReturn.Set->SetIdentityContext(IntReturn.Context) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(IntReturn.CreateType(TEXT("Point"), TEXT("vm-fingerprints"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, IntReturn.Type)));
		ASSERT_THAT(IsTrue(IntReturn.AddIntField(*IntReturn.Type, "x", ttInt, IntReturn.X)));
		ASSERT_THAT(IsTrue(IntReturn.AddIntField(*IntReturn.Type, "y", ttInt, IntReturn.Y)));
		ASSERT_THAT(IsTrue(IntReturn.AddMethod(*IntReturn.Type, "Read", ttInt, IntReturn.Method)));
		ASSERT_THAT(IsTrue(IntReturn.Set->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(IntReturn.Set->Freeze() == asEMetadataResult::Succeeded));

		FPointSet Int64Return;
		ASSERT_THAT(IsTrue(Int64Return.Set->SetIdentityContext(Int64Return.Context) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Int64Return.CreateType(TEXT("Point"), TEXT("vm-fingerprints"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Int64Return.Type)));
		ASSERT_THAT(IsTrue(Int64Return.AddIntField(*Int64Return.Type, "x", ttInt, Int64Return.X)));
		ASSERT_THAT(IsTrue(Int64Return.AddIntField(*Int64Return.Type, "y", ttInt, Int64Return.Y)));
		ASSERT_THAT(IsTrue(Int64Return.AddMethod(*Int64Return.Type, "Read", ttInt64, Int64Return.Method)));
		ASSERT_THAT(IsTrue(Int64Return.Set->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Int64Return.Set->Freeze() == asEMetadataResult::Succeeded));

		ASSERT_THAT(IsTrue(IntReturn.Type->GetStableKey() == Int64Return.Type->GetStableKey()));
		ASSERT_THAT(IsTrue(IntReturn.Method->GetStableKey() == Int64Return.Method->GetStableKey()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Left;
		AS_NAMESPACE_QUALIFIER asSSchemaHash Right;
		AS_NAMESPACE_QUALIFIER asSLayoutHash LeftLayout;
		AS_NAMESPACE_QUALIFIER asSLayoutHash RightLayout;
		TConstArrayView<uint8> LeftWitness;
		TConstArrayView<uint8> RightWitness;
		TConstArrayView<uint8> LeftLayoutWitness;
		TConstArrayView<uint8> RightLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			IntReturn.Type->TryGetSchemaHash(Left, &LeftWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Int64Return.Type->TryGetSchemaHash(Right, &RightWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			IntReturn.Type->TryGetLayoutHash(LeftLayout, &LeftLayoutWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Int64Return.Type->TryGetLayoutHash(RightLayout, &RightLayoutWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(Left, LeftWitness, Right, RightWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Equal,
			CompareLayoutFingerprints(LeftLayout, LeftLayoutWitness, RightLayout, RightLayoutWitness)));
	}

	TEST_METHOD(FunctionBodyAndSourceLocationDoNotChangeTypeFingerprints)
	{
		FPointSet Point;
		ASSERT_THAT(IsTrue(Point.BuildPoint()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash BeforeSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash BeforeLayout;
		TConstArrayView<uint8> BeforeSchemaWitness;
		TConstArrayView<uint8> BeforeLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetSchemaHash(BeforeSchema, &BeforeSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetLayoutHash(BeforeLayout, &BeforeLayoutWitness)));
		TArray<uint8> SchemaCopy(BeforeSchemaWitness.GetData(), BeforeSchemaWitness.Num());
		TArray<uint8> LayoutCopy(BeforeLayoutWitness.GetData(), BeforeLayoutWitness.Num());
		Point.Type->scriptSectionIdx = 9;
		Point.Type->declaredAt = 123;
		AS_NAMESPACE_QUALIFIER asSSchemaHash AfterSchema;
		AS_NAMESPACE_QUALIFIER asSLayoutHash AfterLayout;
		TConstArrayView<uint8> AfterSchemaWitness;
		TConstArrayView<uint8> AfterLayoutWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetSchemaHash(AfterSchema, &AfterSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetLayoutHash(AfterLayout, &AfterLayoutWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Equal,
			CompareSchemaFingerprints(BeforeSchema, SchemaCopy, AfterSchema, AfterSchemaWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Equal,
			CompareLayoutFingerprints(BeforeLayout, LayoutCopy, AfterLayout, AfterLayoutWitness)));
	}

	TEST_METHOD(InjectedEqualDigestsWithDifferentWitnessesAreCollisions)
	{
		FCollidingDigest Provider;
		FPointSet Left;
		FPointSet Right;
		Left.Set->SetFingerprintDigestProvider(&Provider);
		Right.Set->SetFingerprintDigestProvider(&Provider);
		ASSERT_THAT(IsTrue(Left.BuildPoint()));
		ASSERT_THAT(IsTrue(Right.Set->SetIdentityContext(Right.Context) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Right.CreateType(TEXT("Other"), TEXT("vm-fingerprints.other"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right.Type)));
		ASSERT_THAT(IsTrue(Right.AddIntField(*Right.Type, "value", ttInt, Right.X)));
		ASSERT_THAT(IsTrue(Right.Set->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Right.Set->Freeze() == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSSchemaHash LeftHash;
		AS_NAMESPACE_QUALIFIER asSSchemaHash RightHash;
		TConstArrayView<uint8> LeftWitness;
		TConstArrayView<uint8> RightWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Left.Type->TryGetSchemaHash(LeftHash, &LeftWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Right.Type->TryGetSchemaHash(RightHash, &RightWitness)));
		ASSERT_THAT(IsTrue(LeftHash == RightHash));
		ASSERT_THAT(IsFalse(SameBytes(LeftWitness, RightWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Collision,
			CompareSchemaFingerprints(LeftHash, LeftWitness, RightHash, RightWitness)));
	}

	TEST_METHOD(UnauthenticatedShellFreezeRejectsFingerprintQuery)
	{
		auto Set = asCModuleDefinitionSet::Create();
		asCObjectType* Type = nullptr;
		uint8 Bytes[32] = {};
		Bytes[0] = 1;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Key)));
		ASSERT_THAT(IsTrue(Set->CreateObjectType(Key, "Shell", "", asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Type) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Set->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Set->Freeze() == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		TConstArrayView<uint8> Witness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::UnauthenticatedDefinition,
			Type->TryGetSchemaHash(Schema, &Witness)));
		ASSERT_THAT(IsTrue(Schema.Digest.IsZero()));
		ASSERT_THAT(AreEqual(0, Witness.Num()));
	}

	TEST_METHOD(BuildingAndIncompleteStatesRejectFingerprints)
	{
		FPointSet Building;
		ASSERT_THAT(IsTrue(Building.Set->SetIdentityContext(Building.Context) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Building.CreateType(TEXT("Point"), TEXT("vm-fingerprints"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Building.Type)));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::NotReady, Building.Type->TryGetSchemaHash(Schema)));
		ASSERT_THAT(IsTrue(Building.AddIntField(*Building.Type, "x", ttInt, Building.X)));
		ASSERT_THAT(IsTrue(Building.AddIntField(*Building.Type, "y", ttInt, Building.Y)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::NotReady, Building.Type->TryGetSchemaHash(Schema)));
	}

	TEST_METHOD(HandleCycleTerminatesAndValueCyclePublishesNoHash)
	{
		FPointSet Cycle;
		ASSERT_THAT(IsTrue(Cycle.Set->SetIdentityContext(Cycle.Context) == asEMetadataResult::Succeeded));
		asCObjectType* A = nullptr;
		asCObjectType* B = nullptr;
		ASSERT_THAT(IsTrue(Cycle.CreateType(TEXT("A"), TEXT("vm-fingerprints.cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT, A)));
		ASSERT_THAT(IsTrue(Cycle.CreateType(TEXT("B"), TEXT("vm-fingerprints.cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT, B)));
		asCObjectProperty* AB = nullptr;
		asCObjectProperty* BA = nullptr;
		ASSERT_THAT(IsTrue(Cycle.Set->AddProperty(*A, "Other", asCDataType::CreateObjectHandle(B, false), AB) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Cycle.Set->AddProperty(*B, "Other", asCDataType::CreateObjectHandle(A, false), BA) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Cycle.Set->FinalizeLayouts() == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(Cycle.Set->Freeze() == asEMetadataResult::Succeeded));
		AS_NAMESPACE_QUALIFIER asSSchemaHash SchemaA;
		AS_NAMESPACE_QUALIFIER asSSchemaHash SchemaB;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded, A->TryGetSchemaHash(SchemaA)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded, B->TryGetSchemaHash(SchemaB)));

		FPointSet ValueCycle;
		ASSERT_THAT(IsTrue(ValueCycle.Set->SetIdentityContext(ValueCycle.Context) == asEMetadataResult::Succeeded));
		asCObjectType* Left = nullptr;
		asCObjectType* Right = nullptr;
		asCObjectProperty* First = nullptr;
		asCObjectProperty* Second = nullptr;
		ASSERT_THAT(IsTrue(ValueCycle.CreateType(TEXT("Left"), TEXT("vm-fingerprints.value-cycle"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Left)));
		ASSERT_THAT(IsTrue(ValueCycle.CreateType(TEXT("Right"), TEXT("vm-fingerprints.value-cycle"), asOBJ_VALUE | asOBJ_SCRIPT_OBJECT, Right)));
		ASSERT_THAT(IsTrue(ValueCycle.Set->AddProperty(*Left, "Value", asCDataType::CreateType(Right, false), First) == asEMetadataResult::Succeeded));
		ASSERT_THAT(IsTrue(ValueCycle.Set->AddProperty(*Right, "Value", asCDataType::CreateType(Left, false), Second) == asEMetadataResult::Succeeded));
		ASSERT_THAT(AreEqual(asEMetadataResult::RecursiveValueType, ValueCycle.Set->FinalizeLayouts()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Rejected;
		TConstArrayView<uint8> RejectedWitness;
		ASSERT_THAT(AreNotEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Left->TryGetSchemaHash(Rejected, &RejectedWitness)));
		ASSERT_THAT(IsTrue(Rejected.Digest.IsZero()));
		ASSERT_THAT(AreEqual(0, RejectedWitness.Num()));
		ASSERT_THAT(AreEqual(-1, First->byteOffset));
		ASSERT_THAT(AreEqual(-1, Second->byteOffset));
	}

	TEST_METHOD(ConcurrentReadsAgree)
	{
		FPointSet Point;
		ASSERT_THAT(IsTrue(Point.BuildPoint()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Expected;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded, Point.Type->TryGetSchemaHash(Expected)));
		std::atomic<int32> Failures{0};
		ParallelFor(32, [&](int32)
		{
			AS_NAMESPACE_QUALIFIER asSSchemaHash Hash;
			TConstArrayView<uint8> Witness;
			if (Point.Type->TryGetSchemaHash(Hash, &Witness) != AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded
				|| Hash != Expected
				|| Witness.Num() == 0)
			{
				Failures.fetch_add(1);
			}
		});
		ASSERT_THAT(AreEqual(0, Failures.load()));
	}

	TEST_METHOD(RetiredAuthenticDefinitionsRemainInspectable)
	{
		FPointSet Point;
		ASSERT_THAT(IsTrue(Point.BuildPoint()));
		FEngineOwner Engine;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Succeeded, Engine.Ptr->InstallDefinitionSet(MoveTemp(Point.Set))));
		Point.Type->AddRef();
		Engine.Reset();
		ASSERT_THAT(AreEqual(asEDefinitionSetState::Retired, Point.Set->GetState()));
		AS_NAMESPACE_QUALIFIER asSSchemaHash Schema;
		TConstArrayView<uint8> Witness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			Point.Type->TryGetSchemaHash(Schema, &Witness)));
		ASSERT_THAT(IsFalse(Schema.Digest.IsZero()));
		ASSERT_THAT(IsTrue(Witness.Num() > 0));
		ASSERT_THAT(IsNull(Point.Type->GetEngine()));
		ASSERT_THAT(IsTrue(Point.Type->GetTypeId() < 0));
		FEngineOwner Other;
		ASSERT_THAT(AreEqual(asEMetadataRegistrationResult::Retired, Other.Ptr->InstallDefinitionSet(MoveTemp(Point.Set))));
		Point.Type->Release();
	}

	TEST_METHOD(EnumBaseInterfaceGenericAccessAndListChangeSchema)
	{
		auto MakeEnum = [](int Value, asCEnumType*& Out) -> FPointSet
		{
			FPointSet Fixture;
			Fixture.Set->SetIdentityContext(Fixture.Context);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("vm-fingerprints.enum");
			Descriptor.Name = TEXT("Color");
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Enum;
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			Fixture.Context->GetIdentityRegistry().InternNominalType(Descriptor, Key);
			Fixture.Set->CreateEnumType(Key, "Color", "", Out);
			Fixture.Set->AddEnumValue(*Out, "Red", Value);
			Fixture.Set->FinalizeLayouts();
			Fixture.Set->Freeze();
			return Fixture;
		};
		asCEnumType* FirstEnum = nullptr;
		asCEnumType* SecondEnum = nullptr;
		FPointSet EnumA = MakeEnum(1, FirstEnum);
		FPointSet EnumB = MakeEnum(2, SecondEnum);
		AS_NAMESPACE_QUALIFIER asSSchemaHash EnumSchemaA;
		AS_NAMESPACE_QUALIFIER asSSchemaHash EnumSchemaB;
		TConstArrayView<uint8> EnumWitnessA;
		TConstArrayView<uint8> EnumWitnessB;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			FirstEnum->TryGetSchemaHash(EnumSchemaA, &EnumWitnessA)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			SecondEnum->TryGetSchemaHash(EnumSchemaB, &EnumWitnessB)));
		ASSERT_THAT(IsTrue(FirstEnum->GetStableKey() == SecondEnum->GetStableKey()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareSchemaFingerprints(EnumSchemaA, EnumWitnessA, EnumSchemaB, EnumWitnessB)));
	}
};

#endif
