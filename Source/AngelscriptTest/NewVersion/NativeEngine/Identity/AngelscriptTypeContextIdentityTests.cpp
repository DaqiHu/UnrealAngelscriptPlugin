#include "CQTest.h"

#include "frontend/as_type_context.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypeContextIdentityTests
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
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Result;
		Result.StableScope = TEXT("type-context-tests");
		Result.NamespacePath = {TEXT("Gameplay"), TEXT("Types")};
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct;
		Result.Name = Name;
		return Result;
	}

	static AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor PrimitiveDescriptor(
		AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Result;
		Result.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		Result.Primitive = Primitive;
		return Result;
	}

	TEST_CLASS_WITH_FLAGS(
		IdentityTypeContext,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(SharedContextsRetainOneRegistryAndImmutableOptions)
		{
			AS_NAMESPACE_QUALIFIER asSTypeContextOptions Options;
			Options.FloatIsFloat64 = false;
			Options.ExpandDefaultArrayToTemplate = false;
			Options.PointerByteSize = 4;
			Options.TargetProfile = TEXT("wasm32-test");

			TWeakPtr<AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry, ESPMode::ThreadSafe> WeakRegistry;
			TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> FirstContext;
			{
				TSharedRef<AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry, ESPMode::ThreadSafe> Registry =
					MakeShared<AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry, ESPMode::ThreadSafe>();
				WeakRegistry = Registry;
				FirstContext = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(
					Options, Registry);
			}
			ASSERT_THAT(IsTrue(WeakRegistry.IsValid()));
			TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> SecondContext =
				MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(
					Options, FirstContext->GetIdentityRegistryOwner());
			ASSERT_THAT(IsTrue(
				&FirstContext->GetIdentityRegistry() == &SecondContext->GetIdentityRegistry()));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor SharedDescriptor =
				NominalDescriptor(TEXT("SharedAcrossBuilders"));
			AS_NAMESPACE_QUALIFIER asSStableKey SharedKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				FirstContext->GetIdentityRegistry().InternNominalType(SharedDescriptor, SharedKey)));
			FirstContext.Reset();
			ASSERT_THAT(IsTrue(WeakRegistry.IsValid()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				SecondContext->GetIdentityRegistry().ValidateNominalType(
					SharedKey, SharedDescriptor)));
			ASSERT_THAT(IsFalse(SecondContext->GetOptions().FloatIsFloat64));
			ASSERT_THAT(IsFalse(SecondContext->GetOptions().ExpandDefaultArrayToTemplate));
			ASSERT_THAT(AreEqual(4u, SecondContext->GetOptions().PointerByteSize));
			ASSERT_THAT(AreEqual(FString(TEXT("wasm32-test")),
				SecondContext->GetOptions().TargetProfile));
		}

		TEST_METHOD(QualifiedTypeUsesAreCanonicalizedThroughTheirOwningRegistry)
		{
			AS_NAMESPACE_QUALIFIER asCTypeContext Context;
			AS_NAMESPACE_QUALIFIER asSStableKey IntType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.InternPrimitiveTypeUse(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntType)));

			AS_NAMESPACE_QUALIFIER asSStableKey SameType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.InternQualifiedTypeUse(
					IntType, AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::None, SameType)));
			ASSERT_THAT(IsTrue(SameType == IntType));

			AS_NAMESPACE_QUALIFIER asSStableKey ConstType;
			AS_NAMESPACE_QUALIFIER asSStableKey ConstReferenceType;
			AS_NAMESPACE_QUALIFIER asSStableKey Duplicate;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.InternQualifiedTypeUse(
					IntType, AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const, ConstType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.InternQualifiedTypeUse(
					ConstType, AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference, ConstReferenceType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.InternQualifiedTypeUse(
					IntType,
					AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const
						| AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference,
					Duplicate)));
			ASSERT_THAT(IsTrue(ConstReferenceType == Duplicate));

			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Resolved;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Context.GetIdentityRegistry().ResolveTypeUse(ConstReferenceType, Resolved)));
			ASSERT_THAT(AreEqual(
				AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const
					| AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference,
				Resolved.Qualifiers));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive, Resolved.Kind));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, Resolved.Primitive));

			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			const asUINT CountBeforeFailures = Context.GetIdentityRegistry().GetIdentityCount();
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Context.InternQualifiedTypeUse(
					IntType, AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Nullable, Rejected)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRole,
				Context.InternQualifiedTypeUse(
					IntType, AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::ParameterOut, Rejected)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRecord,
				Context.InternQualifiedTypeUse(
					MakeUnknownKey(0x41), AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));
			ASSERT_THAT(AreEqual(
				CountBeforeFailures, Context.GetIdentityRegistry().GetIdentityCount()));
		}

		TEST_METHOD(TypedValidationAuthenticatesExactSemanticDescriptors)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor OwnerDescriptor =
				NominalDescriptor(TEXT("ValidationOwner"));
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(OwnerDescriptor, Owner)));

			const AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor IntDescriptor =
				PrimitiveDescriptor(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor FloatDescriptor =
				PrimitiveDescriptor(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32);
			const AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor VoidDescriptor =
				PrimitiveDescriptor(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void);
			AS_NAMESPACE_QUALIFIER asSStableKey IntType;
			AS_NAMESPACE_QUALIFIER asSStableKey FloatType;
			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(IntDescriptor, IntType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(FloatDescriptor, FloatType)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(VoidDescriptor, VoidType)));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = TEXT("type-context-tests");
			Function.Owner = Owner;
			Function.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method;
			Function.Name = TEXT("ValidateMe");
			Function.Parameters = {{IntType, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In}};
			Function.ReturnType = VoidType;
			AS_NAMESPACE_QUALIFIER asSStableKey FunctionKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Function, FunctionKey)));
			const asUINT IdentityCount = Registry.GetIdentityCount();

			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ValidateNominalType(Owner, OwnerDescriptor)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ValidateTypeUse(IntType, IntDescriptor)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ValidateFunction(FunctionKey, Function)));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor WrongNominal = OwnerDescriptor;
			WrongNominal.Name = TEXT("DifferentName");
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateNominalType(Owner, WrongNominal)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateTypeUse(IntType, FloatDescriptor)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRole,
				Registry.ValidateTypeUse(Owner, IntDescriptor)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidRecord,
				Registry.ValidateNominalType(MakeUnknownKey(0x42), OwnerDescriptor)));

			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor WrongFunction = Function;
			WrongFunction.Name = TEXT("OtherName");
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(FunctionKey, WrongFunction)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor ReturnOnlyConflict = Function;
			ReturnOnlyConflict.ReturnType = FloatType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::ReturnTypeConflict,
				Registry.ValidateFunction(FunctionKey, ReturnOnlyConflict)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor DefinitionOnlyChange = Function;
			DefinitionOnlyChange.DefinitionModifiers =
				AS_NAMESPACE_QUALIFIER asEFunctionDefinitionModifiers::Static
				| AS_NAMESPACE_QUALIFIER asEFunctionDefinitionModifiers::Final;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateFunction(FunctionKey, DefinitionOnlyChange)));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor ResolvedNominal;
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor ResolvedTypeUse;
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor ResolvedFunction;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveNominalType(Owner, ResolvedNominal)));
			ASSERT_THAT(AreEqual(OwnerDescriptor.Name, ResolvedNominal.Name));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveTypeUse(IntType, ResolvedTypeUse)));
			ASSERT_THAT(AreEqual(IntDescriptor.Primitive, ResolvedTypeUse.Primitive));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveFunction(FunctionKey, ResolvedFunction)));
			ASSERT_THAT(AreEqual(Function.Name, ResolvedFunction.Name));
			ASSERT_THAT(IsTrue(Function.ReturnType == ResolvedFunction.ReturnType));
			ASSERT_THAT(AreEqual(IdentityCount, Registry.GetIdentityCount()));
		}

		TEST_METHOD(DeclarationCoordinatesUseConstrainedOwnerRoles)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Module;
			Module.StableScope = TEXT("type-context-tests");
			Module.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Module;
			Module.Name = TEXT("GameplayModule");
			AS_NAMESPACE_QUALIFIER asSStableKey ModuleKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Module, ModuleKey)));

			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Namespace;
			Namespace.StableScope = Module.StableScope;
			Namespace.Owner = ModuleKey;
			Namespace.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Namespace;
			Namespace.Name = TEXT("Gameplay");
			AS_NAMESPACE_QUALIFIER asSStableKey NamespaceKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Namespace, NamespaceKey)));

			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Global;
			Global.StableScope = Module.StableScope;
			Global.Owner = NamespaceKey;
			Global.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::GlobalVariable;
			Global.Name = TEXT("Difficulty");
			AS_NAMESPACE_QUALIFIER asSStableKey GlobalKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Global, GlobalKey)));

			AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor OwnerDescriptor =
				NominalDescriptor(TEXT("Inventory"));
			AS_NAMESPACE_QUALIFIER asSStableKey Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternNominalType(OwnerDescriptor, Owner)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Property;
			Property.StableScope = Module.StableScope;
			Property.Owner = Owner;
			Property.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Property;
			Property.Name = TEXT("Capacity");
			AS_NAMESPACE_QUALIFIER asSStableKey PropertyKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Property, PropertyKey)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Enumerator = Property;
			Enumerator.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::EnumConstant;
			Enumerator.Name = TEXT("Rare");
			AS_NAMESPACE_QUALIFIER asSStableKey EnumeratorKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Enumerator, EnumeratorKey)));

			AS_NAMESPACE_QUALIFIER asSStableKey VoidType;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternTypeUse(
					PrimitiveDescriptor(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void), VoidType)));
			AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Function;
			Function.StableScope = Module.StableScope;
			Function.Kind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global;
			Function.Name = TEXT("SetDifficulty");
			Function.ReturnType = VoidType;
			AS_NAMESPACE_QUALIFIER asSStableKey FunctionKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternFunction(Function, FunctionKey)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Parameter;
			Parameter.StableScope = Module.StableScope;
			Parameter.Owner = FunctionKey;
			Parameter.Kind = AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Parameter;
			Parameter.Ordinal = 0;
			AS_NAMESPACE_QUALIFIER asSStableKey ParameterKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(Parameter, ParameterKey)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor SecondParameter = Parameter;
			SecondParameter.Ordinal = 1;
			AS_NAMESPACE_QUALIFIER asSStableKey SecondParameterKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.InternDeclaration(SecondParameter, SecondParameterKey)));
			ASSERT_THAT(IsFalse(ParameterKey == SecondParameterKey));

			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor Resolved;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveDeclaration(PropertyKey, Resolved)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclarationIdentityKind::Property, Resolved.Kind));
			ASSERT_THAT(AreEqual(FString(TEXT("Capacity")), Resolved.Name));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ValidateDeclaration(PropertyKey, Property)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor WrongProperty = Property;
			WrongProperty.Name = TEXT("OtherProperty");
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::DescriptorMismatch,
				Registry.ValidateDeclaration(PropertyKey, WrongProperty)));

			AS_NAMESPACE_QUALIFIER asSStableKey Rejected;
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor WrongOwner = Property;
			WrongOwner.Owner = ModuleKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternDeclaration(WrongOwner, Rejected)));
			WrongOwner = Parameter;
			WrongOwner.Owner = Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternDeclaration(WrongOwner, Rejected)));
			WrongOwner = Namespace;
			WrongOwner.Owner = Owner;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternDeclaration(WrongOwner, Rejected)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor NamedParameter = Parameter;
			NamedParameter.Name = TEXT("ExcludedParameterName");
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidDescriptor,
				Registry.InternDeclaration(NamedParameter, Rejected)));
			AS_NAMESPACE_QUALIFIER asSDeclarationIdentityDescriptor OwnedModule = Module;
			OwnedModule.Owner = ModuleKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::InvalidOwner,
				Registry.InternDeclaration(OwnedModule, Rejected)));
			ASSERT_THAT(IsTrue(Rejected.IsZero()));

			AS_NAMESPACE_QUALIFIER asEStableIdentityRole Role = AS_NAMESPACE_QUALIFIER asEStableIdentityRole::Invalid;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded,
				Registry.ResolveIdentityRole(ModuleKey, Role)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStableIdentityRole::Declaration, Role));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
