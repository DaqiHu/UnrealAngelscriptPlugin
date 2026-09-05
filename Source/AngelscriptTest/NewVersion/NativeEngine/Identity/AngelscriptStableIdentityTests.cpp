#include "CQTest.h"
#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace StableIdentityTests
{
	class FCollidingDigestProvider final : public AS_NAMESPACE_QUALIFIER asIStableKeyDigestProvider
	{
	public:
		explicit FCollidingDigestProvider(AS_NAMESPACE_QUALIFIER asSStableKey InKey)
			: Key(InKey)
		{
		}

		AS_NAMESPACE_QUALIFIER asSStableKey ComputeDigest(
			TConstArrayView<uint8> CanonicalDescription) const override
		{
			check(!CanonicalDescription.IsEmpty());
			return Key;
		}

	private:
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
	};

	static AS_NAMESPACE_QUALIFIER asSStableKey MakeInjectedKey(uint8 Marker)
	{
		uint8 Bytes[AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount] = {};
		Bytes[0] = Marker;
		AS_NAMESPACE_QUALIFIER asSStableKey Result;
		const bool bCreated = AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Result);
		check(bCreated);
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor NominalDescriptor(const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Result;
		Result.StableScope = TEXT("identity-tests");
		Result.NamespacePath = {TEXT("Gameplay"), TEXT("Types")};
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		Result.Name = Name;
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor PrimitiveUse(
		AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Result;
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		Result.Primitive = Primitive;
		return Result;
	}

	TEST_CLASS_WITH_FLAGS(StableIdentity, "Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(PublicKeyIsOneOpaqueBlake3Value)
		{
			ASSERT_THAT(AreEqual(32, static_cast<int32>(sizeof(AS_NAMESPACE_QUALIFIER asSStableKey))));
			const AS_NAMESPACE_QUALIFIER asSStableKey Key = MakeInjectedKey(0x2a);
			ASSERT_THAT(IsFalse(Key.IsZero()));
			ASSERT_THAT(AreEqual(64, Key.ToHexString().Len()));
			ASSERT_THAT(AreEqual(32, Key.GetBytes().Num()));
		}

		TEST_METHOD(NominalIdentityUsesStableSemanticFieldsOnly)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor BaselineDescriptor =
				NominalDescriptor(TEXT("Inventory"));
			AS_NAMESPACE_QUALIFIER asSStableKey Baseline;
			AS_NAMESPACE_QUALIFIER asSStableKey Duplicate;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(BaselineDescriptor, Baseline)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(BaselineDescriptor, Duplicate)));
			ASSERT_THAT(IsTrue(Baseline == Duplicate));
			ASSERT_THAT(AreEqual(1u, Registry.GetIdentityCount()));

			TArray<AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor> SemanticVariants;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Scope = BaselineDescriptor;
			Scope.StableScope = TEXT("other-module");
			SemanticVariants.Add(Scope);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Namespace = BaselineDescriptor;
			Namespace.NamespacePath = {TEXT("Editor")};
			SemanticVariants.Add(Namespace);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Kind = BaselineDescriptor;
			Kind.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
			SemanticVariants.Add(Kind);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Name = BaselineDescriptor;
			Name.Name = TEXT("Equipment");
			SemanticVariants.Add(Name);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Arity = BaselineDescriptor;
			Arity.GenericArity = 1;
			SemanticVariants.Add(Arity);

			for (const AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor& Variant : SemanticVariants)
			{
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
					Registry.InternNominalType(Variant, Key)));
				ASSERT_THAT(IsFalse(Key == Baseline));
			}
		}

		TEST_METHOD(StableKeysDoNotDependOnRegistryInsertionOrder)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry FirstRegistry;
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry SecondRegistry;
			AS_NAMESPACE_QUALIFIER asSStableKey Ignored;
			AS_NAMESPACE_QUALIFIER asSStableKey First;
			AS_NAMESPACE_QUALIFIER asSStableKey Second;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				FirstRegistry.InternNominalType(NominalDescriptor(TEXT("Unrelated")), Ignored)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				FirstRegistry.InternNominalType(NominalDescriptor(TEXT("Target")), First)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				SecondRegistry.InternNominalType(NominalDescriptor(TEXT("Target")), Second)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				SecondRegistry.InternNominalType(NominalDescriptor(TEXT("Unrelated")), Ignored)));
			ASSERT_THAT(IsTrue(First == Second));
		}

		TEST_METHOD(UnstableAndInvalidNominalOriginsNeverAcquireAKey)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			for (const AS_NAMESPACE_QUALIFIER asETypeDeclOrigin Origin : {
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Local,
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Anonymous,
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Recovery})
			{
				AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor = NominalDescriptor(TEXT("Transient"));
				Descriptor.Origin = Origin;
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::NonCacheable,
					Registry.InternNominalType(Descriptor, Key)));
				ASSERT_THAT(IsTrue(Key.IsZero()));
			}
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Invalid = NominalDescriptor(TEXT("Invalid"));
			Invalid.Origin = static_cast<AS_NAMESPACE_QUALIFIER asETypeDeclOrigin>(255);
			AS_NAMESPACE_QUALIFIER asSStableKey InvalidKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.InternNominalType(Invalid, InvalidKey)));
			ASSERT_THAT(IsTrue(InvalidKey.IsZero()));
			ASSERT_THAT(AreEqual(0u, Registry.GetIdentityCount()));
		}

		TEST_METHOD(DelegateAndEventCallableTypesHaveDistinctNominalIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Delegate = NominalDescriptor(TEXT("OnChanged"));
			Delegate.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Delegate;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Event = Delegate;
			Event.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Event;
			AS_NAMESPACE_QUALIFIER asSStableKey DelegateKey;
			AS_NAMESPACE_QUALIFIER asSStableKey EventKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Delegate, DelegateKey)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Event, EventKey)));
			ASSERT_THAT(IsFalse(DelegateKey == EventKey));
		}

		TEST_METHOD(NestedNominalIdentityUsesItsOwnersStableKey)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey FirstOwner;
			AS_NAMESPACE_QUALIFIER asSStableKey SecondOwner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("FirstOwner")), FirstOwner)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("SecondOwner")), SecondOwner)));
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Nested = NominalDescriptor(TEXT("Nested"));
			Nested.Owner = FirstOwner;
			AS_NAMESPACE_QUALIFIER asSStableKey FirstNested;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Nested, FirstNested)));
			Nested.Owner = SecondOwner;
			AS_NAMESPACE_QUALIFIER asSStableKey SecondNested;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Nested, SecondNested)));
			ASSERT_THAT(IsFalse(FirstNested == SecondNested));
		}

		TEST_METHOD(TypeUsesContainOrderedKeyEdgesWithoutRecursiveWitnesses)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey NodeDeclaration;
			AS_NAMESPACE_QUALIFIER asSStableKey PairDeclaration;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("Node")), NodeDeclaration)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("Pair")), PairDeclaration)));

			AS_NAMESPACE_QUALIFIER asSStableKey IntType;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), IntType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32), FloatType)));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor RecursiveHandle;
			RecursiveHandle.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			RecursiveHandle.Declaration = NodeDeclaration;
			RecursiveHandle.Qualifiers = AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Handle;
			AS_NAMESPACE_QUALIFIER asSStableKey NodeHandle;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(RecursiveHandle, NodeHandle)));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Pair;
			Pair.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			Pair.Declaration = PairDeclaration;
			Pair.Arguments = {IntType, FloatType};
			AS_NAMESPACE_QUALIFIER asSStableKey IntFloat;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Pair, IntFloat)));
			Pair.Arguments = {FloatType, IntType};
			AS_NAMESPACE_QUALIFIER asSStableKey FloatInt;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Pair, FloatInt)));

			ASSERT_THAT(IsFalse(NodeHandle.IsZero()));
			ASSERT_THAT(IsFalse(IntFloat == FloatInt));
			ASSERT_THAT(AreEqual(7u, Registry.GetIdentityCount()));
		}

		TEST_METHOD(UnknownTypeEdgesFailWithoutMutatingTheRegistry)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Use;
			Use.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			Use.Declaration = MakeInjectedKey(0x73);
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRecord,
				Registry.InternTypeUse(Use, Key)));
			ASSERT_THAT(IsTrue(Key.IsZero()));
			ASSERT_THAT(AreEqual(0u, Registry.GetIdentityCount()));
		}

		TEST_METHOD(CallableTypeUseOwnsItsResolvedSignatureStructure)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey IntType;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), IntType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32), FloatType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void), VoidType)));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Callable;
			Callable.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Callable.ReturnType = VoidType;
			Callable.Parameters = {{IntType}, {FloatType}};
			AS_NAMESPACE_QUALIFIER asSStableKey IntFloat;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Callable, IntFloat)));
			Callable.Parameters = {{FloatType}, {IntType}};
			AS_NAMESPACE_QUALIFIER asSStableKey FloatInt;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Callable, FloatInt)));
			Callable.Parameters = {{IntType}, {FloatType}};
			Callable.ReturnType = FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey DifferentReturn;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Callable, DifferentReturn)));
			ASSERT_THAT(IsFalse(IntFloat == FloatInt));
			ASSERT_THAT(IsFalse(IntFloat == DifferentReturn));
		}

		TEST_METHOD(FunctionIdentityUsesCanonicalSignatureAndRejectsReturnOnlyOverloads)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("Inventory")), Owner)));
			AS_NAMESPACE_QUALIFIER asSStableKey IntType;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), IntType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32), FloatType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void), VoidType)));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = TEXT("identity-tests");
			Function.Owner = Owner;
			Function.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			Function.Name = TEXT("Add");
			Function.Parameters = {{IntType}};
			Function.ReturnType = VoidType;
			AS_NAMESPACE_QUALIFIER asSStableKey Baseline;
			AS_NAMESPACE_QUALIFIER asSStableKey Duplicate;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Function, Baseline)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Function, Duplicate)));
			ASSERT_THAT(IsTrue(Baseline == Duplicate));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor DifferentParameter = Function;
			DifferentParameter.Parameters = {{FloatType}};
			AS_NAMESPACE_QUALIFIER asSStableKey OtherOverload;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(DifferentParameter, OtherOverload)));
			ASSERT_THAT(IsFalse(Baseline == OtherOverload));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor OutParameter = Function;
			OutParameter.Parameters[0].PassingMode = AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Out;
			AS_NAMESPACE_QUALIFIER asSStableKey OutKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(OutParameter, OutKey)));
			ASSERT_THAT(IsFalse(Baseline == OutKey));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor ConstOverload = Function;
			ConstOverload.Modifiers = AS_NAMESPACE_QUALIFIER asEFunctionIdentityModifiers::ConstMethod;
			AS_NAMESPACE_QUALIFIER asSStableKey ConstKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(ConstOverload, ConstKey)));
			ASSERT_THAT(IsFalse(Baseline == ConstKey));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor DefinitionRevision = Function;
			DefinitionRevision.DefinitionModifiers =
				AS_NAMESPACE_QUALIFIER asEFunctionDefinitionModifiers::Static
				| AS_NAMESPACE_QUALIFIER asEFunctionDefinitionModifiers::Final;
			AS_NAMESPACE_QUALIFIER asSStableKey DefinitionRevisionKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(DefinitionRevision, DefinitionRevisionKey)));
			ASSERT_THAT(IsTrue(Baseline == DefinitionRevisionKey));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor ReturnOnlyConflict = DefinitionRevision;
			ReturnOnlyConflict.ReturnType = FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::ReturnTypeConflict,
				Registry.InternFunction(ReturnOnlyConflict, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
			ASSERT_THAT(AreEqual(8u, Registry.GetIdentityCount()));
		}

		TEST_METHOD(CanonicalRegistryRejectsAnInjectedCollisionAndRetainsTheFirstDescription)
		{
			const AS_NAMESPACE_QUALIFIER asSStableKey Injected = MakeInjectedKey(0xb7);
			const FCollidingDigestProvider DigestProvider(Injected);
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry(DigestProvider);
			AS_NAMESPACE_QUALIFIER asSStableKey First;
			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("First")), First)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Collision,
				Registry.InternNominalType(NominalDescriptor(TEXT("Second")), Rejected)));
			ASSERT_THAT(IsTrue(First == Injected));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
			ASSERT_THAT(AreEqual(1u, Registry.GetIdentityCount()));
			AS_NAMESPACE_QUALIFIER asEStableIdentityRole Role = AS_NAMESPACE_QUALIFIER asEStableIdentityRole::Invalid;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveIdentityRole(First, Role)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStableIdentityRole::NominalType, Role));
		}

		TEST_METHOD(CoreTypeAndFunctionAdaptersPreserveTheCanonicalKeyExactly)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("AdapterOwner")), Owner)));
			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(PrimitiveUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void), VoidType)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = TEXT("identity-tests");
			Function.Owner = Owner;
			Function.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			Function.Name = TEXT("Reset");
			Function.ReturnType = VoidType;
			AS_NAMESPACE_QUALIFIER asSStableKey FunctionKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Function, FunctionKey)));

			const FAngelscriptStableTypeKey CoreType =
				FAngelscriptArtifactIdentityBuilder::ProjectTypeKey(Owner);
			const FAngelscriptStableFunctionKey CoreFunction =
				FAngelscriptArtifactIdentityBuilder::ProjectFunctionKey(FunctionKey);
			ASSERT_THAT(AreEqual(Owner.ToHexString(), CoreType.Hash.ToHexString()));
			ASSERT_THAT(AreEqual(FunctionKey.ToHexString(), CoreFunction.Hash.ToHexString()));
		}
	};
}

#endif
