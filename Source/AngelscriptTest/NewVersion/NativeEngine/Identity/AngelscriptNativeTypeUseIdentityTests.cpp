#include "CQTest.h"

#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeUseIdentityTests
{
	static AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor NominalDeclaration(const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("game");
		Descriptor.NamespacePath = {TEXT("Types")};
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = Name;
		return Descriptor;
	}

	static AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Primitive(
		AS_NAMESPACE_QUALIFIER asEPrimitiveType PrimitiveType)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		Descriptor.Primitive = PrimitiveType;
		return Descriptor;
	}

	static AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Nominal(
		const AS_NAMESPACE_QUALIFIER asSStableKey& Declaration,
		TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Arguments = {})
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
		Descriptor.Declaration = Declaration;
		Descriptor.Arguments = MoveTemp(Arguments);
		return Descriptor;
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey InternDeclaration(
		AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry,
		const TCHAR* Name,
		AS_NAMESPACE_QUALIFIER asETypeDeclKind Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor = NominalDeclaration(Name);
		Descriptor.Kind = Kind;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		check(Registry.InternNominalType(Descriptor, Key)
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

	TEST_CLASS_WITH_FLAGS(
		IdentityTypeUse,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(PrimitiveAndNominalUsesEncodeDeterministically)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey IntFirst;
			AS_NAMESPACE_QUALIFIER asSStableKey IntSecond;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Primitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), IntFirst)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Primitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32), IntSecond)));
			ASSERT_THAT(IsTrue(IntFirst == IntSecond));

			const AS_NAMESPACE_QUALIFIER asSStableKey ObjectDeclaration =
				InternDeclaration(Registry, TEXT("Object"));
			AS_NAMESPACE_QUALIFIER asSStableKey Object;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(ObjectDeclaration), Object)));
			ASSERT_THAT(IsFalse(IntFirst == Object));
		}

		TEST_METHOD(OrderedGenericArgumentsRemainStableKeyEdges)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey PairDeclaration =
				InternDeclaration(Registry, TEXT("Pair"));
			const AS_NAMESPACE_QUALIFIER asSStableKey BoxDeclaration =
				InternDeclaration(Registry, TEXT("Box"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);

			AS_NAMESPACE_QUALIFIER asSStableKey IntFloat;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatInt;
			AS_NAMESPACE_QUALIFIER asSStableKey Nested;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(PairDeclaration, {IntType, FloatType}), IntFloat)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(PairDeclaration, {FloatType, IntType}), FloatInt)));
			ASSERT_THAT(IsFalse(IntFloat == FloatInt));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(BoxDeclaration, {IntFloat}), Nested)));
			ASSERT_THAT(IsFalse(Nested == IntFloat));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Resolved;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveTypeUse(Nested, Resolved)));
			ASSERT_THAT(AreEqual(1, Resolved.Arguments.Num()));
			ASSERT_THAT(IsTrue(Resolved.Arguments[0] == IntFloat));
		}

		TEST_METHOD(LegalUseQualifiersHaveIndependentIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey ObjectDeclaration =
				InternDeclaration(Registry, TEXT("Object"));
			const TArray<AS_NAMESPACE_QUALIFIER asETypeUseQualifiers> Qualifiers = {
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::None,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Handle,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Handle
					| AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Nullable,
			};
			TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Keys;
			for (const AS_NAMESPACE_QUALIFIER asETypeUseQualifiers Qualifier : Qualifiers)
			{
				AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor = Nominal(ObjectDeclaration);
				Descriptor.Qualifiers = Qualifier;
				AS_NAMESPACE_QUALIFIER asSStableKey& Key = Keys.AddDefaulted_GetRef();
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
					Registry.InternTypeUse(Descriptor, Key)));
			}
			for (int32 Left = 0; Left < Keys.Num(); ++Left)
			{
				for (int32 Right = Left + 1; Right < Keys.Num(); ++Right)
				{
					ASSERT_THAT(IsFalse(Keys[Left] == Keys[Right]));
				}
			}
		}

		TEST_METHOD(AliasSpellingIsNotASecondIdentityAuthority)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey Target = InternDeclaration(Registry, TEXT("Vector"));
			AS_NAMESPACE_QUALIFIER asSStableKey Direct;
			AS_NAMESPACE_QUALIFIER asSStableKey AuthoredAliasProjection;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(Target), Direct)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(Target), AuthoredAliasProjection)));
			ASSERT_THAT(IsTrue(Direct == AuthoredAliasProjection));

			const AS_NAMESPACE_QUALIFIER asSStableKey StrongAliasDeclaration = InternDeclaration(
				Registry, TEXT("Position"), AS_NAMESPACE_QUALIFIER asETypeDeclKind::Typedef);
			AS_NAMESPACE_QUALIFIER asSStableKey StrongAlias;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Nominal(StrongAliasDeclaration), StrongAlias)));
			ASSERT_THAT(IsFalse(Direct == StrongAlias));
		}

		TEST_METHOD(InvalidQualifierRolesAndUnknownEdgesFailClosed)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			for (const AS_NAMESPACE_QUALIFIER asETypeUseQualifiers InvalidRole : {
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::ParameterOut,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::OwnershipTransfer,
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::TargetABILowering})
			{
				AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor =
					Primitive(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
				Descriptor.Qualifiers = InvalidRole;
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRole,
					Registry.InternTypeUse(Descriptor, Key)));
				ASSERT_THAT(IsTrue(Key.IsZero()));
			}

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Unknown =
				Nominal(AS_NAMESPACE_QUALIFIER asSStableKey());
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.InternTypeUse(Unknown, Key)));
			const AS_NAMESPACE_QUALIFIER asSStableKey PrimitiveKey =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRole,
				Registry.InternTypeUse(Nominal(PrimitiveKey), Key)));
		}

		TEST_METHOD(ResourceBudgetsAndCallableStructureAreEnforced)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSStableKey ManyDeclaration =
				InternDeclaration(Registry, TEXT("Many"));
			const AS_NAMESPACE_QUALIFIER asSStableKey IntType =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor TooMany = Nominal(ManyDeclaration);
			TooMany.Arguments.Init(
				IntType, AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry::MaxDescriptorEdges + 1);
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::BudgetExceeded,
				Registry.InternTypeUse(TooMany, Key)));

			const AS_NAMESPACE_QUALIFIER asSStableKey FloatType =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);
			const AS_NAMESPACE_QUALIFIER asSStableKey VoidType =
				InternPrimitive(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void);
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Callable;
			Callable.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Callable.ReturnType = VoidType;
			Callable.Parameters = {
				{IntType, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In},
				{FloatType, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Out},
			};
			AS_NAMESPACE_QUALIFIER asSStableKey First;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Callable, First)));
			Callable.Parameters[1].PassingMode = AS_NAMESPACE_QUALIFIER asEParameterPassingMode::InOut;
			AS_NAMESPACE_QUALIFIER asSStableKey DifferentPassingMode;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(Callable, DifferentPassingMode)));
			ASSERT_THAT(IsFalse(First == DifferentPassingMode));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
