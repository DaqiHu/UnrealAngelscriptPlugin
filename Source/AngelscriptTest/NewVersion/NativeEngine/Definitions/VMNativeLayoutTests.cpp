#include "CQTest.h"
#include "as_metadata_fingerprint.h"
#include "as_metadata_image.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "frontend/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

struct alignas(8) FNativeRecord
{
	uint8 Tag;
	int32 Value;
	double Weight;
};

struct FNativeInner
{
	int32 Value;
};

struct FNativeOuterInline
{
	FNativeInner Inner;
};

struct FNativeOuterIndirect
{
	FNativeInner* Inner;
};

TEST_CLASS_WITH_FLAGS(VMNativeLayout, "Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNativeType
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
			MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
		TUniquePtr<asCMetadataImage> Image = asCMetadataImage::Create();
		asCObjectType* Type = nullptr;

		bool CreateNative(const TCHAR* Name, asUINT Size, asUINT Alignment)
		{
			if (Image->SetIdentityContext(Context) != asEMetadataResult::Succeeded) return false;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
			Descriptor.StableScope = TEXT("vm-native-layout");
			Descriptor.Name = Name;
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			return Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded
				&& Image->CreateObjectType(Key, TCHAR_TO_UTF8(Name), "", asOBJ_VALUE | asOBJ_APP_CLASS, Type) == asEMetadataResult::Succeeded
				&& Image->SetNativeLayout(*Type, Size, Alignment) == asEMetadataResult::Succeeded;
		}

		asEMetadataResult Direct(const char* Name, eTokenType Token, int Offset, asCObjectProperty*& Out)
		{
			asSNativePropertyAddress Address;
			Address.Mode = asENativeAddressMode::Direct;
			Address.Offset = Offset;
			return Image->DefineNativeProperty(*Type, Name, Image->Primitive(Token), Address, Out);
		}
	};

public:
	TEST_METHOD(NativeRecordPreservesHostOffsetsAndPadding)
	{
		FNativeType Fixture;
		ASSERT_THAT(IsTrue(Fixture.CreateNative(TEXT("NativeRecord"), sizeof(FNativeRecord), alignof(FNativeRecord))));
		asCObjectProperty* Tag = nullptr;
		asCObjectProperty* Value = nullptr;
		asCObjectProperty* Weight = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Tag", ttUInt8, static_cast<int>(offsetof(FNativeRecord, Tag)), Tag)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Value", ttInt, static_cast<int>(offsetof(FNativeRecord, Value)), Value)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Weight", ttFloat64, static_cast<int>(offsetof(FNativeRecord, Weight)), Weight)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->Freeze()));
		ASSERT_THAT(AreEqual(static_cast<int>(sizeof(FNativeRecord)), Fixture.Type->size));
		ASSERT_THAT(AreEqual(static_cast<int>(alignof(FNativeRecord)), Fixture.Type->alignment));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FNativeRecord, Tag)), Tag->byteOffset));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FNativeRecord, Value)), Value->byteOffset));
		ASSERT_THAT(AreEqual(static_cast<int>(offsetof(FNativeRecord, Weight)), Weight->byteOffset));

		FNativeRecord Record;
		FMemory::Memset(&Record, 0xAA, sizeof(Record));
		Record.Tag = 7;
		Record.Value = 42;
		Record.Weight = 1.5;
		uint8* Base = reinterpret_cast<uint8*>(&Record);
		ASSERT_THAT(AreEqual(static_cast<uint8>(7), *reinterpret_cast<uint8*>(Base + Tag->byteOffset)));
		ASSERT_THAT(AreEqual(42, *reinterpret_cast<int32*>(Base + Value->byteOffset)));
		ASSERT_THAT(IsTrue(Record.Weight == *reinterpret_cast<double*>(Base + Weight->byteOffset)));
		for (int32 Index = 1; Index < static_cast<int32>(offsetof(FNativeRecord, Value)); ++Index)
		{
			ASSERT_THAT(AreEqual(static_cast<uint8>(0xAA), Base[Index]));
		}
	}

	TEST_METHOD(CompositeInlineAndIndirectHaveDistinctLayoutWitnesses)
	{
		FNativeType InlineType;
		ASSERT_THAT(IsTrue(InlineType.CreateNative(TEXT("OuterInline"), sizeof(FNativeOuterInline), alignof(FNativeOuterInline))));
		asCObjectType* InnerType = nullptr;
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor InnerDesc;
		InnerDesc.StableScope = TEXT("vm-native-layout");
		InnerDesc.Name = TEXT("Inner");
		InnerDesc.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		AS_NAMESPACE_QUALIFIER asSStableKey InnerKey;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
			InlineType.Context->GetIdentityRegistry().InternNominalType(InnerDesc, InnerKey)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			InlineType.Image->CreateObjectType(InnerKey, "Inner", "", asOBJ_VALUE | asOBJ_APP_CLASS, InnerType)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			InlineType.Image->SetNativeLayout(*InnerType, sizeof(FNativeInner), alignof(FNativeInner))));
		asCObjectProperty* InnerValue = nullptr;
		asSNativePropertyAddress InnerAddress;
		InnerAddress.Mode = asENativeAddressMode::Direct;
		InnerAddress.Offset = static_cast<int>(offsetof(FNativeInner, Value));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			InlineType.Image->DefineNativeProperty(*InnerType, "Value", InlineType.Image->Primitive(ttInt), InnerAddress, InnerValue)));
		asCObjectProperty* InlineMember = nullptr;
		asSNativePropertyAddress InlineAddress;
		InlineAddress.Mode = asENativeAddressMode::CompositeInline;
		InlineAddress.Offset = static_cast<int>(offsetof(FNativeOuterInline, Inner));
		InlineAddress.MemberOffset = static_cast<int>(offsetof(FNativeInner, Value));
		InlineAddress.CompositeStorageType = InnerType;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			InlineType.Image->DefineNativeProperty(*InlineType.Type, "Value", InlineType.Image->Primitive(ttInt), InlineAddress, InlineMember)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, InlineType.Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, InlineType.Image->Freeze()));

		FNativeType IndirectType;
		ASSERT_THAT(IsTrue(IndirectType.CreateNative(TEXT("OuterIndirect"), sizeof(FNativeOuterIndirect), alignof(FNativeOuterIndirect))));
		asCObjectType* IndirectInner = nullptr;
		AS_NAMESPACE_QUALIFIER asSStableKey IndirectInnerKey;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
			IndirectType.Context->GetIdentityRegistry().InternNominalType(InnerDesc, IndirectInnerKey)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			IndirectType.Image->CreateObjectType(IndirectInnerKey, "Inner", "", asOBJ_VALUE | asOBJ_APP_CLASS, IndirectInner)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			IndirectType.Image->SetNativeLayout(*IndirectInner, sizeof(FNativeInner), alignof(FNativeInner))));
		asCObjectProperty* IndirectInnerValue = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			IndirectType.Image->DefineNativeProperty(*IndirectInner, "Value", IndirectType.Image->Primitive(ttInt), InnerAddress, IndirectInnerValue)));
		asCObjectProperty* IndirectMember = nullptr;
		asSNativePropertyAddress IndirectAddress;
		IndirectAddress.Mode = asENativeAddressMode::CompositeIndirect;
		IndirectAddress.Offset = static_cast<int>(offsetof(FNativeOuterIndirect, Inner));
		IndirectAddress.MemberOffset = static_cast<int>(offsetof(FNativeInner, Value));
		IndirectAddress.CompositeStorageType = IndirectInner;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			IndirectType.Image->DefineNativeProperty(*IndirectType.Type, "Value", IndirectType.Image->Primitive(ttInt), IndirectAddress, IndirectMember)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, IndirectType.Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, IndirectType.Image->Freeze()));

		AS_NAMESPACE_QUALIFIER asSLayoutHash InlineHash;
		AS_NAMESPACE_QUALIFIER asSLayoutHash IndirectHash;
		TConstArrayView<uint8> InlineWitness;
		TConstArrayView<uint8> IndirectWitness;
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			InlineType.Type->TryGetLayoutHash(InlineHash, &InlineWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded,
			IndirectType.Type->TryGetLayoutHash(IndirectHash, &IndirectWitness)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareLayoutFingerprints(InlineHash, InlineWitness, IndirectHash, IndirectWitness)));
		ASSERT_THAT(IsTrue(InlineMember->nativeAddressMode == asENativeAddressMode::CompositeInline));
		ASSERT_THAT(IsTrue(IndirectMember->nativeAddressMode == asENativeAddressMode::CompositeIndirect));
	}

	TEST_METHOD(NativeAdmissionRejectsOverflowOverlapPackedAndFrozenMutation)
	{
		FNativeType Fixture;
		ASSERT_THAT(IsTrue(Fixture.CreateNative(TEXT("NativeRecord"), sizeof(FNativeRecord), alignof(FNativeRecord))));
		asCObjectProperty* Rejected = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidLayout,
			Fixture.Direct("Overflow", ttInt, static_cast<int>(sizeof(FNativeRecord)), Rejected)));
		ASSERT_THAT(IsNull(Rejected));
		asCObjectProperty* Tag = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Tag", ttUInt8, static_cast<int>(offsetof(FNativeRecord, Tag)), Tag)));
		asCObjectProperty* Overlap = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidLayout,
			Fixture.Direct("Overlap", ttInt, static_cast<int>(offsetof(FNativeRecord, Tag)), Overlap)));
		ASSERT_THAT(IsNull(Overlap));
		asCObjectProperty* Packed = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidLayout, Fixture.Direct("Packed", ttInt, 1, Packed)));
		asCObjectProperty* Duplicate = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::DuplicateName,
			Fixture.Direct("Tag", ttUInt8, static_cast<int>(offsetof(FNativeRecord, Tag)), Duplicate)));
		ASSERT_THAT(AreEqual(asUINT(1), Fixture.Type->localProperties.GetLength()));
		asCObjectProperty* Value = nullptr;
		asCObjectProperty* Weight = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Value", ttInt, static_cast<int>(offsetof(FNativeRecord, Value)), Value)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded,
			Fixture.Direct("Weight", ttFloat64, static_cast<int>(offsetof(FNativeRecord, Weight)), Weight)));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->FinalizeLayouts()));
		ASSERT_THAT(AreEqual(asEMetadataResult::Succeeded, Fixture.Image->Freeze()));
		asCObjectProperty* Late = nullptr;
		ASSERT_THAT(AreEqual(asEMetadataResult::InvalidState,
			Fixture.Direct("Late", ttInt, static_cast<int>(offsetof(FNativeRecord, Value)), Late)));
		ASSERT_THAT(AreEqual(asUINT(3), Fixture.Type->localProperties.GetLength()));
	}

	TEST_METHOD(SameKeyDifferentOffsetChangesLayoutHash)
	{
		auto Build = [](int ValueOffset, asCObjectType*& OutType, AS_NAMESPACE_QUALIFIER asSLayoutHash& OutHash, TArray<uint8>& OutWitness) -> bool
		{
			FNativeType Fixture;
			if (!Fixture.CreateNative(TEXT("NativeRecord"), sizeof(FNativeRecord), alignof(FNativeRecord))) return false;
			asCObjectProperty* Tag = nullptr;
			asCObjectProperty* Value = nullptr;
			if (Fixture.Direct("Tag", ttUInt8, static_cast<int>(offsetof(FNativeRecord, Tag)), Tag) != asEMetadataResult::Succeeded) return false;
			if (Fixture.Direct("Value", ttInt, ValueOffset, Value) != asEMetadataResult::Succeeded) return false;
			if (Fixture.Image->FinalizeLayouts() != asEMetadataResult::Succeeded) return false;
			if (Fixture.Image->Freeze() != asEMetadataResult::Succeeded) return false;
			TConstArrayView<uint8> Witness;
			if (Fixture.Type->TryGetLayoutHash(OutHash, &Witness) != AS_NAMESPACE_QUALIFIER asEFingerprintStatus::Succeeded) return false;
			OutWitness.Append(Witness.GetData(), Witness.Num());
			OutType = Fixture.Type;
			Fixture.Type->AddRef();
			return true;
		};

		asCObjectType* First = nullptr;
		asCObjectType* Second = nullptr;
		AS_NAMESPACE_QUALIFIER asSLayoutHash FirstHash;
		AS_NAMESPACE_QUALIFIER asSLayoutHash SecondHash;
		TArray<uint8> FirstWitness;
		TArray<uint8> SecondWitness;
		ASSERT_THAT(IsTrue(Build(static_cast<int>(offsetof(FNativeRecord, Value)), First, FirstHash, FirstWitness)));
		ASSERT_THAT(IsTrue(Build(static_cast<int>(offsetof(FNativeRecord, Weight)), Second, SecondHash, SecondWitness)));
		ASSERT_THAT(IsTrue(First->GetStableKey() == Second->GetStableKey()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFingerprintEquality::Different,
			CompareLayoutFingerprints(FirstHash, FirstWitness, SecondHash, SecondWitness)));
		First->Release();
		Second->Release();
	}
};

#endif
