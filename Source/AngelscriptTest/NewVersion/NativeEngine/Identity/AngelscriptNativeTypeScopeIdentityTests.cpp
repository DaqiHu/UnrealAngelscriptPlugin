#include "CQTest.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "as_runtime_type_binding.h"
#include "frontend/as_type_identity.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeScopeIdentityTests
{
	static AS_NAMESPACE_QUALIFIER asSStableKey MakeUnknownKey(uint8 Marker)
	{
		uint8 Bytes[AS_NAMESPACE_QUALIFIER asSStableKey::ByteCount] = {};
		Bytes[0] = Marker;
		AS_NAMESPACE_QUALIFIER asSStableKey Result;
		check(AS_NAMESPACE_QUALIFIER asSStableKey::TryCreate(MakeArrayView(Bytes), Result));
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor NominalDescriptor(const TCHAR* Name)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("scope-tests");
		Descriptor.NamespacePath = {TEXT("Types")};
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = Name;
		return Descriptor;
	}

	TEST_CLASS_WITH_FLAGS(
		IdentityScope,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(RuntimeTypeIdsAreGenerationLocalAndNeverDurable)
		{
			const asSRuntimeTypeGeneration FirstGeneration{601};
			const asSRuntimeTypeGeneration SecondGeneration{602};
			const asSRuntimeTypeId First{FirstGeneration, 17};
			const asSRuntimeTypeId Second{SecondGeneration, 42};
			ASSERT_THAT(AreEqual(
				asRUNTIME_TYPE_BIND_OK,
				asValidateRuntimeTypeIdOwner(First, FirstGeneration)));
			ASSERT_THAT(AreEqual(
				asRUNTIME_TYPE_BIND_INVALID_OWNER,
				asValidateRuntimeTypeIdOwner(First, SecondGeneration)));
			ASSERT_THAT(IsFalse(First.Equals(Second)));
			ASSERT_THAT(AreEqual(
				asRUNTIME_TYPE_BIND_INVALID_DURABLE_ID,
				asRejectRuntimeTypeIdForDurableEncoding(First)));
		}

		TEST_METHOD(RuntimeProjectionRequiresTheOwningTypeUseRegistry)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey Declaration;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("Alpha")), Declaration)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor UseDescriptor;
			UseDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			UseDescriptor.Declaration = Declaration;
			AS_NAMESPACE_QUALIFIER asSStableKey TypeUse;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(UseDescriptor, TypeUse)));

			asSTypeABIKey Projected;
			ASSERT_THAT(AreEqual(asRUNTIME_TYPE_BIND_OK,
				asProjectCanonicalTypeUse(Registry, TypeUse, Projected)));
			ASSERT_THAT(IsTrue(Projected.canonicalTypeUse == TypeUse));

			const AS_NAMESPACE_QUALIFIER asSStableKey Sentinel = MakeUnknownKey(0x8d);
			asSTypeABIKey Unchanged;
			Unchanged.canonicalTypeUse = Sentinel;
			ASSERT_THAT(AreEqual(asRUNTIME_TYPE_BIND_INVALID,
				asProjectCanonicalTypeUse(Registry, Declaration, Unchanged)));
			ASSERT_THAT(IsTrue(Unchanged.canonicalTypeUse == Sentinel));
			ASSERT_THAT(AreEqual(asRUNTIME_TYPE_BIND_INVALID,
				asProjectCanonicalTypeUse(Registry, MakeUnknownKey(0x8e), Unchanged)));
			ASSERT_THAT(IsTrue(Unchanged.canonicalTypeUse == Sentinel));
		}

		TEST_METHOD(CoreTypeAndFunctionAdaptersAreLosslessNotAuthenticators)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(NominalDescriptor(TEXT("Owner")), Owner)));
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor VoidDescriptor;
			VoidDescriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			VoidDescriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void;
			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(VoidDescriptor, VoidType)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = TEXT("scope-tests");
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

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
