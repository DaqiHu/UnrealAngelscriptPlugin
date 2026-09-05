#include "CQTest.h"

#include "frontend/as_type_identity.h"

#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace ConversionIdentityTests
{
	static AS_NAMESPACE_QUALIFIER asSStableKey MakeInjectedKey(uint8 Marker)
	{
		uint8 Bytes[AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount] = {};
		Bytes[0] = Marker;
		AS_NAMESPACE_QUALIFIER asSStableKey Result;
		check(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Result));
		return Result;
	}

	class FFunctionCollisionDigestProvider final
		: public AS_NAMESPACE_QUALIFIER asIStableKeyDigestProvider
	{
	public:
		explicit FFunctionCollisionDigestProvider(AS_NAMESPACE_QUALIFIER asSStableKey InFunctionKey)
			: FunctionKey(InFunctionKey)
		{
		}

		AS_NAMESPACE_QUALIFIER asSStableKey ComputeDigest(
			TConstArrayView<uint8> CanonicalDescription) const override
		{
			check(CanonicalDescription.Num() > 7);
			if (CanonicalDescription[7]
				== static_cast<uint8>(AS_NAMESPACE_QUALIFIER asECanonicalIdentityDomain::Function))
			{
				return FunctionKey;
			}

			const FBlake3Hash Hash = FBlake3::HashBuffer(
				CanonicalDescription.GetData(),
				static_cast<uint64>(CanonicalDescription.Num()));
			AS_NAMESPACE_QUALIFIER asSStableKey Result;
			check(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(
				MakeArrayView(Hash.GetBytes(), AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount), Result));
			return Result;
		}

	private:
		AS_NAMESPACE_QUALIFIER asSStableKey FunctionKey;
	};

	static AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Nominal(const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Result;
		Result.StableScope = TEXT("conversion-identity-tests");
		Result.NamespacePath = {TEXT("Gameplay"), TEXT("Conversions")};
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		Result.Name = Name;
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Primitive(
		AS_NAMESPACE_QUALIFIER asEPrimitiveType PrimitiveType)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Result;
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		Result.Primitive = PrimitiveType;
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey InternNominal(
		AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry,
		const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		check(Registry.InternNominalType(Nominal(Name), Key)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded);
		return Key;
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey InternPrimitive(
		AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry,
		AS_NAMESPACE_QUALIFIER asEPrimitiveType PrimitiveType)
	{
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		check(Registry.InternTypeUse(Primitive(PrimitiveType), Key)
			== AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded);
		return Key;
	}

	static AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Conversion(
		const AS_NAMESPACE_QUALIFIER asSStableKey& Owner,
		const AS_NAMESPACE_QUALIFIER asSStableKey& Destination,
		const TCHAR* Name = TEXT("opImplConv"))
	{
		AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Result;
		Result.StableScope = TEXT("conversion-identity-tests");
		Result.NamespacePath = {TEXT("Gameplay"), TEXT("Conversions")};
		Result.Owner = Owner;
		Result.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Conversion;
		Result.Name = Name;
		Result.ReturnType = Destination;
		Result.ConversionDestination = Destination;
		return Result;
	}

	TEST_CLASS_WITH_FLAGS(
		ConversionIdentity,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ConversionKindIsAppendOnlyAndAllMaintainedNamesAreAdmitted)
		{
			ASSERT_THAT(AreEqual(
				static_cast<uint8>(
					static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::ClassDefault) + 1),
				static_cast<uint8>(AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Conversion)));

			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Keys;
			for (const TCHAR* Name : {
				TEXT("opConv"), TEXT("opImplConv"), TEXT("opCast"), TEXT("opImplCast")})
			{
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
					Registry.InternFunction(Conversion(Owner, IntType, Name), Key)));
				ASSERT_THAT(IsFalse(Key.IsZero()));
				Keys.Add(Key);
			}
			ASSERT_THAT(IsFalse(Keys[0] == Keys[1]));
			ASSERT_THAT(IsFalse(Keys[1] == Keys[2]));
			ASSERT_THAT(IsFalse(Keys[2] == Keys[3]));
		}

		TEST_METHOD(DestinationAndReceiverQualificationAreIdentitySignificant)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);

			AS_NAMESPACE_QUALIFIER asSStableKey IntKey;
			AS_NAMESPACE_QUALIFIER asSStableKey IntDuplicate;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatKey;
			AS_NAMESPACE_QUALIFIER asSStableKey ConstIntKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Conversion(Owner, IntType), IntKey)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Conversion(Owner, IntType), IntDuplicate)));
			ASSERT_THAT(IsTrue(IntKey == IntDuplicate));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Conversion(Owner, FloatType), FloatKey)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor ConstInt = Conversion(Owner, IntType);
			ConstInt.Modifiers = AS_NAMESPACE_QUALIFIER asEFunctionIdentityModifiers::ConstMethod;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(ConstInt, ConstIntKey)));
			ASSERT_THAT(IsFalse(IntKey == FloatKey));
			ASSERT_THAT(IsFalse(IntKey == ConstIntKey));
			ASSERT_THAT(AreEqual(6u, Registry.GetIdentityCount()));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Resolved;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveFunction(FloatKey, Resolved)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Conversion, Resolved.Kind));
			ASSERT_THAT(IsTrue(Resolved.Owner == Owner));
			ASSERT_THAT(IsTrue(Resolved.ReturnType == FloatType));
			ASSERT_THAT(IsTrue(Resolved.ConversionDestination == FloatType));
			ASSERT_THAT(IsTrue(Resolved.Parameters.IsEmpty()));
		}

		TEST_METHOD(OrdinaryReturnOnlyConflictContractRemainsUnchanged)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Ordinary;
			Ordinary.StableScope = TEXT("conversion-identity-tests");
			Ordinary.Owner = Owner;
			Ordinary.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			Ordinary.Name = TEXT("Value");
			Ordinary.ReturnType = IntType;
			AS_NAMESPACE_QUALIFIER asSStableKey OrdinaryKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Ordinary, OrdinaryKey)));
			Ordinary.ReturnType = FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::ReturnTypeConflict,
				Registry.InternFunction(Ordinary, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));

			Ordinary.ReturnType = IntType;
			Ordinary.ConversionDestination = IntType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.InternFunction(Ordinary, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
		}

		TEST_METHOD(ConversionShapeRejectsSyntheticOrVoidSignaturesWithoutPublishing)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey VoidType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void);
			const asUINT InitialCount = Registry.GetIdentityCount();

			TArray<AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor> Invalid;
			Invalid.Add(Conversion(Owner, IntType, TEXT("ConvertToInt")));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Parameter = Conversion(Owner, IntType);
			Parameter.Parameters = {{IntType}};
			Invalid.Add(Parameter);
			Invalid.Add(Conversion(Owner, VoidType));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Mismatch = Conversion(Owner, IntType);
			Mismatch.ConversionDestination = VoidType;
			Invalid.Add(Mismatch);

			for (const AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor& Descriptor : Invalid)
			{
				AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
					Registry.InternFunction(Descriptor, Rejected)));
				ASSERT_THAT(IsTrue(Rejected.IsZero()));
			}
			ASSERT_THAT(AreEqual(InitialCount, Registry.GetIdentityCount()));
		}

		TEST_METHOD(ConversionEdgesAuthenticateOwnerRoleAndDestinationRole)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const asUINT InitialCount = Registry.GetIdentityCount();

			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor MissingOwner = Conversion({}, IntType);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternFunction(MissingOwner, Rejected)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongOwner = Conversion(IntType, IntType);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternFunction(WrongOwner, Rejected)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongDestination = Conversion(Owner, Owner);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRole,
				Registry.InternFunction(WrongDestination, Rejected)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor UnknownDestination =
				Conversion(Owner, MakeInjectedKey(0x51));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRecord,
				Registry.InternFunction(UnknownDestination, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
			ASSERT_THAT(AreEqual(InitialCount, Registry.GetIdentityCount()));
		}

		TEST_METHOD(ValidationAuthenticatesKindOwnerDestinationAndDefinitionFacts)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey OtherOwner = InternNominal(Registry, TEXT("Other"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);
			const AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Baseline = Conversion(Owner, IntType);
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Baseline, Key)));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongKind = Baseline;
			WrongKind.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			WrongKind.ConversionDestination = {};
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(Key, WrongKind)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongOwner = Baseline;
			WrongOwner.Owner = OtherOwner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(Key, WrongOwner)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongDestination = Conversion(Owner, FloatType);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(Key, WrongDestination)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor MismatchedEdge = Baseline;
			MismatchedEdge.ConversionDestination = FloatType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.ValidateFunction(Key, MismatchedEdge)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongDefinitionFacts = Baseline;
			WrongDefinitionFacts.DefinitionModifiers =
				AS_NAMESPACE_QUALIFIER asEFunctionDefinitionModifiers::Explicit;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(Key, WrongDefinitionFacts)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ValidateFunction(Key, Baseline)));
		}

		TEST_METHOD(DestinationDistinctConversionsRejectInjectedDigestCollision)
		{
			const AS_NAMESPACE_QUALIFIER asSStableKey Injected = MakeInjectedKey(0xc6);
			const FFunctionCollisionDigestProvider DigestProvider(Injected);
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry(DigestProvider);
			const AS_NAMESPACE_QUALIFIER asSStableKey Owner = InternNominal(Registry, TEXT("Source"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType = InternPrimitive(
				Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);

			AS_NAMESPACE_QUALIFIER asSStableKey First;
			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Conversion(Owner, IntType), First)));
			ASSERT_THAT(IsTrue(First == Injected));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Collision,
				Registry.InternFunction(Conversion(Owner, FloatType), Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
			ASSERT_THAT(AreEqual(4u, Registry.GetIdentityCount()));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Resolved;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveFunction(First, Resolved)));
			ASSERT_THAT(IsTrue(Resolved.ConversionDestination == IntType));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
