#include "CQTest.h"

#include "frontend/Sema/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeDeclIdentityTests
{
	static AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor MakeDescriptor()
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("game");
		Descriptor.NamespacePath = {TEXT("Gameplay"), TEXT("Items")};
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = TEXT("InventoryItem");
		return Descriptor;
	}

	TEST_CLASS_WITH_FLAGS(
		IdentityTypeDecl,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(NominalIdentityIsDeterministicAndSemantic)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey Baseline;
			AS_NAMESPACE_QUALIFIER asSStableKey Duplicate;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(MakeDescriptor(), Baseline)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(MakeDescriptor(), Duplicate)));
			ASSERT_THAT(IsTrue(Baseline == Duplicate));
			ASSERT_THAT(AreEqual(1u, Registry.GetIdentityCount()));

			TArray<AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor> Variants;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Scope = MakeDescriptor();
			Scope.StableScope = TEXT("plugin");
			Variants.Add(Scope);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Namespace = MakeDescriptor();
			Namespace.NamespacePath = {TEXT("Editor")};
			Variants.Add(Namespace);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Kind = MakeDescriptor();
			Kind.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
			Variants.Add(Kind);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Name = MakeDescriptor();
			Name.Name = TEXT("Weapon");
			Variants.Add(Name);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Arity = MakeDescriptor();
			Arity.GenericArity = 1;
			Variants.Add(Arity);

			for (const AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor& Variant : Variants)
			{
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
					Registry.InternNominalType(Variant, Key)));
				ASSERT_THAT(IsFalse(Baseline == Key));
			}
		}

		TEST_METHOD(UnstableDeclarationsAreExplicitlyNonCacheable)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			for (const AS_NAMESPACE_QUALIFIER asETypeDeclOrigin Origin : {
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Local,
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Anonymous,
				AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::Recovery})
			{
				AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor = MakeDescriptor();
				Descriptor.Origin = Origin;
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::NonCacheable,
					Registry.InternNominalType(Descriptor, Key)));
				ASSERT_THAT(IsTrue(Key.IsZero()));
			}
			ASSERT_THAT(AreEqual(0u, Registry.GetIdentityCount()));
		}

		TEST_METHOD(GeneratedNominalsRequireAStableGeneratorIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Generated = MakeDescriptor();
			Generated.Origin = AS_NAMESPACE_QUALIFIER asETypeDeclOrigin::StableGenerated;
			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.InternNominalType(Generated, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));

			Generated.StableGeneratorIdentity = TEXT("reflection:v4:InventoryItem");
			AS_NAMESPACE_QUALIFIER asSStableKey First;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Generated, First)));
			Generated.StableGeneratorIdentity = TEXT("reflection:v5:InventoryItem");
			AS_NAMESPACE_QUALIFIER asSStableKey Second;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(Generated, Second)));
			ASSERT_THAT(IsFalse(First == Second));
		}

		TEST_METHOD(InvalidEnumsAndBudgetsFailWithoutPublishingKeys)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			TArray<AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor> InvalidDescriptors;
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor InvalidKind = MakeDescriptor();
			InvalidKind.Kind = static_cast<AS_NAMESPACE_QUALIFIER asETypeDeclKind>(255);
			InvalidDescriptors.Add(InvalidKind);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor InvalidOrigin = MakeDescriptor();
			InvalidOrigin.Origin = static_cast<AS_NAMESPACE_QUALIFIER asETypeDeclOrigin>(255);
			InvalidDescriptors.Add(InvalidOrigin);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor EmptyScope = MakeDescriptor();
			EmptyScope.StableScope.Reset();
			InvalidDescriptors.Add(EmptyScope);
			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor EmptyPathSegment = MakeDescriptor();
			EmptyPathSegment.NamespacePath = {TEXT("Gameplay"), TEXT("")};
			InvalidDescriptors.Add(EmptyPathSegment);

			for (const AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor& Descriptor : InvalidDescriptors)
			{
				AS_NAMESPACE_QUALIFIER asSStableKey Key;
				ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
					Registry.InternNominalType(Descriptor, Key)));
				ASSERT_THAT(IsTrue(Key.IsZero()));
			}

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor TooManyNamespaces = MakeDescriptor();
			TooManyNamespaces.NamespacePath.SetNum(
				AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry::MaxDescriptorEdges + 1);
			for (FString& Segment : TooManyNamespaces.NamespacePath)
			{
				Segment = TEXT("N");
			}
			AS_NAMESPACE_QUALIFIER asSStableKey Key;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::BudgetExceeded,
				Registry.InternNominalType(TooManyNamespaces, Key)));
			ASSERT_THAT(AreEqual(0u, Registry.GetIdentityCount()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
