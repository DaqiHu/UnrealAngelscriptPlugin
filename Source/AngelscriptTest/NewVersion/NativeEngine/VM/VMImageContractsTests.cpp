#include "CQTest.h"
#include "../Definitions/NativeDetachedDefinitionTestSupport.h"
#include "as_bytecode_image_builder.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_metadata_image.h"
#include "frontend/as_canonical_encoding.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(VMImageContracts, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asSByteCodeOperand Imm16(int16 Value)
	{
		asSByteCodeOperand Operand;
		Operand.Kind = asEByteCodeOperandKind::ImmediateS16;
		Operand.Immediate = Value;
		return Operand;
	}

	static bool FinishWithRet(asCByteCodeImageBuilder& Builder, const asSStableKey& Key, TUniquePtr<asCByteCodeImage>& Image)
	{
		if (Builder.BeginFunction(Key, Key, 4, 0) != asEByteCodeImageStatus::Succeeded) return false;
		asSByteCodeOperand Ret = Imm16(0);
		if (Builder.Emit(asBC_RET, MakeArrayView(&Ret, 1)) != asEByteCodeImageStatus::Succeeded) return false;
		if (Builder.EndFunction() != asEByteCodeImageStatus::Succeeded) return false;
		return Builder.Finish(Image) == asEByteCodeImageStatus::Succeeded;
	}

	static const asSByteCodeRequirement* FindRole(const asCByteCodeImage& Image, asEByteCodeRequirementRole Role, const asSStableKey& Key)
	{
		for (const asSByteCodeRequirement& Requirement : Image.Requirements)
		{
			if (Requirement.Role == Role && Requirement.Key == Key) return &Requirement;
		}
		return nullptr;
	}

	static void EncodeIndependentContract(const asSByteCodeRequirement& Requirement, TArray<uint8>& Out)
	{
		AS_NAMESPACE_QUALIFIER asCCanonicalEncoding Encoding(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Schema);
		Encoding.WriteUInt8(static_cast<uint8>(Requirement.Role));
		Encoding.WriteBytes(Requirement.Key.GetBytes());
		Encoding.WriteBytes(Requirement.Schema.Digest.GetBytes());
		Encoding.WriteBytes(Requirement.Layout.Digest.GetBytes());
		Encoding.WriteUInt32(static_cast<uint32>(Requirement.SchemaWitness.Num()));
		Encoding.WriteBytes(MakeArrayView(Requirement.SchemaWitness));
		Encoding.WriteUInt32(static_cast<uint32>(Requirement.LayoutWitness.Num()));
		Encoding.WriteBytes(MakeArrayView(Requirement.LayoutWitness));
		Encoding.WriteBytes(Requirement.SignatureKey.GetBytes());
		Encoding.WriteBytes(Requirement.ReturnTypeUse.GetBytes());
		Encoding.WriteUInt32(Requirement.ReturnDwords);
		Encoding.WriteUInt8(static_cast<uint8>(Requirement.ReturnStorageKind));
		Encoding.WriteUInt32(static_cast<uint32>(Requirement.Parameters.Num()));
		for (const asSByteCodeParameterContract& Parameter : Requirement.Parameters)
		{
			Encoding.WriteBytes(Parameter.TypeUse.GetBytes());
			Encoding.WriteUInt8(Parameter.PassingMode);
			Encoding.WriteUInt32(Parameter.StorageDwords);
			Encoding.WriteUInt8(static_cast<uint8>(Parameter.StorageKind));
		}
		Encoding.WriteUInt32(Requirement.CallableTraits);
		Encoding.WriteBytes(Requirement.OwnerKey.GetBytes());
		Encoding.WriteUInt32(static_cast<uint32>(Requirement.NativeOffset));
		Encoding.WriteUInt8(Requirement.NativeAddressMode);
		TArray<asSStableKey> Sorted = Requirement.ReferencedKeys;
		Sorted.Sort([](const asSStableKey& Left, const asSStableKey& Right)
		{
			return FMemory::Memcmp(Left.GetBytes().GetData(), Right.GetBytes().GetData(), asSStableKey::ByteCount) < 0;
		});
		Encoding.WriteUInt32(static_cast<uint32>(Sorted.Num()));
		for (const asSStableKey& Key : Sorted) Encoding.WriteBytes(Key.GetBytes());
		Out.Reset();
		Out.Append(Encoding.GetBytes().GetData(), Encoding.GetBytes().Num());
	}

public:
	TEST_METHOD(CompleteClosureCapturesFieldBaseReturnAndParameter)
	{
		FDetachedDefinitionFixture Host;
		Host.Scope = TEXT("vm-image-dep");
		asCObjectType* Dep = Host.DefineType(TEXT("Dep"), TEXT("ns"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsNotNull(Dep));
		ASSERT_THAT(IsTrue(Host.FinalizeAndFreeze()));

		FDetachedDefinitionFixture Fixture;
		Fixture.Scope = TEXT("vm-image-main");
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->AddDependency(Host.Image)));
		asCObjectType* FieldType = Fixture.DefineType(TEXT("FieldType"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Base = Fixture.DefineType(TEXT("Base"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectType* Owner = Fixture.DefineType(TEXT("Owner"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->SetBaseType(*Owner, *Base)));
		asCDataType Handle = asCDataType::CreateObjectHandle(FieldType, false);
		Fixture.DefineProperty(*Owner, "Field", Handle);
		asSMetadataParameter Param;
		Param.Name = "value";
		Param.Type = Fixture.Image->Primitive(ttInt);
		asCScriptFunction* Method = Fixture.DefineFunction(
			TEXT("Use"), TEXT(""), asFUNC_SCRIPT, asCDataType::CreateObjectHandle(FieldType, false), MakeArrayView(&Param, 1), Owner);
		ASSERT_THAT(IsNotNull(Method));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));

		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		asUINT DepSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Method, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*Dep, DepSlot)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(FinishWithRet(Builder, Method->GetStableKey(), Image)));
		ASSERT_THAT(IsNotNull(FindRole(*Image, asEByteCodeRequirementRole::Type, Owner->GetStableKey())));
		ASSERT_THAT(IsNotNull(FindRole(*Image, asEByteCodeRequirementRole::Type, Base->GetStableKey())));
		ASSERT_THAT(IsNotNull(FindRole(*Image, asEByteCodeRequirementRole::Type, FieldType->GetStableKey())));
		ASSERT_THAT(IsNotNull(FindRole(*Image, asEByteCodeRequirementRole::Type, Dep->GetStableKey())));
		const asSByteCodeRequirement* Function = FindRole(*Image, asEByteCodeRequirementRole::Function, Method->GetStableKey());
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsFalse(Function->ReturnTypeUse.IsZero()));
		ASSERT_THAT(AreEqual(1, Function->Parameters.Num()));
		ASSERT_THAT(IsFalse(Function->Parameters[0].TypeUse.IsZero()));
		ASSERT_THAT(IsTrue(Function->Key != Function->SignatureKey));
	}

	TEST_METHOD(CyclicHandlesTerminate)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* A = Fixture.DefineType(TEXT("A"), TEXT("cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectType* B = Fixture.DefineType(TEXT("B"), TEXT("cycle"), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		asCObjectProperty* Unused = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->AddProperty(*A, "Other", asCDataType::CreateObjectHandle(B, false), Unused)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Image->AddProperty(*B, "Other", asCDataType::CreateObjectHandle(A, false), Unused)));
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddTypeRequirement(*A, Slot)));
		asSStableKey Key;
		uint8 Bytes[32] = {};
		Bytes[0] = 9;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), Key);
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(FinishWithRet(Builder, Key, Image)));
		ASSERT_THAT(IsTrue(Image->Requirements.Num() >= 2));
	}

	TEST_METHOD(SameFunctionKeyDifferentReturnHasDifferentContracts)
	{
		FDetachedDefinitionFixture IntReturn;
		IntReturn.Scope = TEXT("vm-image-return");
		asCScriptFunction* Left = IntReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, IntReturn.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(IntReturn.FinalizeAndFreeze()));
		FDetachedDefinitionFixture DoubleReturn;
		DoubleReturn.Scope = TEXT("vm-image-return");
		asCScriptFunction* Right = DoubleReturn.DefineFunction(
			TEXT("Read"), TEXT(""), asFUNC_SCRIPT, DoubleReturn.Image->Primitive(ttFloat64), {});
		ASSERT_THAT(IsTrue(DoubleReturn.FinalizeAndFreeze()));
		ASSERT_THAT(IsTrue(Left->GetStableKey() == Right->GetStableKey()));
		asCByteCodeImageBuilder LeftBuilder;
		asUINT LeftSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, LeftBuilder.AddFunctionRequirement(*Left, LeftSlot)));
		asCByteCodeImageBuilder RightBuilder;
		asUINT RightSlot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, RightBuilder.AddFunctionRequirement(*Right, RightSlot)));
		TUniquePtr<asCByteCodeImage> LeftImage;
		TUniquePtr<asCByteCodeImage> RightImage;
		ASSERT_THAT(IsTrue(FinishWithRet(LeftBuilder, Left->GetStableKey(), LeftImage)));
		ASSERT_THAT(IsTrue(FinishWithRet(RightBuilder, Right->GetStableKey(), RightImage)));
		const asSByteCodeRequirement* LeftReq = FindRole(*LeftImage, asEByteCodeRequirementRole::Function, Left->GetStableKey());
		const asSByteCodeRequirement* RightReq = FindRole(*RightImage, asEByteCodeRequirementRole::Function, Right->GetStableKey());
		ASSERT_THAT(IsNotNull(LeftReq));
		ASSERT_THAT(IsNotNull(RightReq));
		ASSERT_THAT(IsTrue(LeftReq->Key == RightReq->Key));
		ASSERT_THAT(IsTrue(LeftReq->SignatureKey != RightReq->SignatureKey));
		ASSERT_THAT(IsTrue(LeftReq->ReturnTypeUse != RightReq->ReturnTypeUse));
		ASSERT_THAT(IsTrue(LeftReq->ContractWitness != RightReq->ContractWitness));
	}

	TEST_METHOD(DuplicateRoleKeyDifferentWitnessRejects)
	{
		asSByteCodeRequirement First;
		First.Role = asEByteCodeRequirementRole::Type;
		uint8 Bytes[32] = {};
		Bytes[0] = 4;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), First.Key);
		First.SchemaWitness.Add(1);
		asSByteCodeRequirement Second = First;
		uint8 Alt[32] = {};
		Alt[0] = 8;
		asSStableKey::TryCreate(MakeArrayView(Alt, 32), Second.SignatureKey);
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(First, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Duplicate, Builder.AddRequirement(Second, Slot)));
	}

	TEST_METHOD(InjectedEqualDigestsDoNotHideWitnessMismatch)
	{
		asSByteCodeRequirement First;
		First.Role = asEByteCodeRequirementRole::Type;
		uint8 Bytes[32] = {};
		Bytes[0] = 5;
		asSStableKey::TryCreate(MakeArrayView(Bytes, 32), First.Key);
		uint8 DigestBytes[32];
		FMemory::Memset(DigestBytes, 0x22, 32);
		asSFingerprintDigest::TryCreate(MakeArrayView(DigestBytes, 32), First.Schema.Digest);
		First.SchemaWitness.Append(DigestBytes, 4);
		asSByteCodeRequirement Second = First;
		Second.SchemaWitness[0] = 0x33;
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddRequirement(First, Slot)));
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::CorruptWitness, Builder.AddRequirement(Second, Slot)));
	}

	TEST_METHOD(UnfrozenTypeRejectsFingerprintQuery)
	{
		FDetachedDefinitionFixture Fixture;
		asCObjectType* Type = Fixture.DefineType(TEXT("Draft"), TEXT(""), asOBJ_REF | asOBJ_SCRIPT_OBJECT);
		ASSERT_THAT(IsNotNull(Type));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::CorruptWitness, Builder.AddTypeRequirement(*Type, Slot)));
	}

	TEST_METHOD(CanonicalContractBytesMatchDocumentedOrder)
	{
		FDetachedDefinitionFixture Fixture;
		asCScriptFunction* Function = Fixture.DefineFunction(
			TEXT("Run"), TEXT(""), asFUNC_SCRIPT, Fixture.Image->Primitive(ttInt), {});
		ASSERT_THAT(IsTrue(Fixture.FinalizeAndFreeze()));
		asCByteCodeImageBuilder Builder;
		asUINT Slot = 0;
		ASSERT_THAT(AreEqual(asEByteCodeImageStatus::Succeeded, Builder.AddFunctionRequirement(*Function, Slot)));
		TUniquePtr<asCByteCodeImage> Image;
		ASSERT_THAT(IsTrue(FinishWithRet(Builder, Function->GetStableKey(), Image)));
		const asSByteCodeRequirement* Requirement = FindRole(*Image, asEByteCodeRequirementRole::Function, Function->GetStableKey());
		ASSERT_THAT(IsNotNull(Requirement));
		ASSERT_THAT(IsTrue(Requirement->ContractWitness.Num() > 0));
		TArray<uint8> Independent;
		EncodeIndependentContract(*Requirement, Independent);
		ASSERT_THAT(AreEqual(Independent.Num(), Requirement->ContractWitness.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Independent.GetData(), Requirement->ContractWitness.GetData(), Independent.Num()) == 0));
	}
};

#endif
