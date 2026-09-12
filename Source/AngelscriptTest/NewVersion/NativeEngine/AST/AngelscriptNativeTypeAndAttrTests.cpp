#include "CQTest.h"

#include "frontend/AST/as_attr.h"
#include "frontend/AST/as_type.h"
#include "frontend/AST/as_type_loc.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTTypeAttrTests
{
	static AS_NAMESPACE_QUALIFIER asCSourceRange Range(asUINT Begin, asUINT End)
	{
		return {{11, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin}, {11, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End}};
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey DeclKey(AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry, const TCHAR* Name, asUINT Arity = 0)
	{
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = TEXT("game");
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		Descriptor.Name = Name;
		Descriptor.GenericArity = Arity;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		Registry.InternNominalType(Descriptor, Key);
		return Key;
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey NominalUse(AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry, const AS_NAMESPACE_QUALIFIER asSStableKey& Decl, TArray<AS_NAMESPACE_QUALIFIER asSStableKey> Arguments = {})
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
		Descriptor.Declaration = Decl;
		Descriptor.Arguments = MoveTemp(Arguments);
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		Registry.InternTypeUse(Descriptor, Key);
		return Key;
	}

	static AS_NAMESPACE_QUALIFIER asSStableKey PrimitiveUse(AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry& Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType Primitive)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
		Descriptor.Primitive = Primitive;
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		Registry.InternTypeUse(Descriptor, Key);
		return Key;
	}

	TEST_CLASS_WITH_FLAGS(
		ASTTypeAndAttr,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(CanonicalTypeFormsHaveConcretePayload)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const auto IntKey = PrimitiveUse(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const auto VectorDecl = DeclKey(Registry, TEXT("Vector"), 1);
			const auto VectorUse = NominalUse(Registry, VectorDecl);
			const auto VectorOfInt = NominalUse(Registry, VectorDecl, {IntKey});
			AS_NAMESPACE_QUALIFIER asCBuiltinType IntType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntKey);
			AS_NAMESPACE_QUALIFIER asCNominalType VectorType(VectorDecl, VectorUse);
			AS_NAMESPACE_QUALIFIER asCQualType IntUse;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(&IntType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntUse)));
			AS_NAMESPACE_QUALIFIER asCGenericType ArrayType(&VectorType, {IntUse}, VectorOfInt);
			AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Signature;
			Signature.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Callable;
			Signature.ReturnType = IntKey;
			Signature.Parameters = {{IntKey, AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Value}};
			AS_NAMESPACE_QUALIFIER asSStableKey SignatureKey;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded, Registry.InternTypeUse(Signature, SignatureKey)));
			AS_NAMESPACE_QUALIFIER asCFunctionType FunctionType(IntUse, {IntUse}, SignatureKey);

			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Builtin, IntType.GetKind()));
			ASSERT_THAT(IsTrue(VectorType.GetDeclarationIdentity() == VectorDecl));
			ASSERT_THAT(IsFalse(VectorOfInt.IsZero()));
			ASSERT_THAT(IsTrue(VectorOfInt != VectorUse && SignatureKey != VectorOfInt));
			ASSERT_THAT(AreEqual(1, ArrayType.GetArguments().Num()));
			ASSERT_THAT(AreEqual(1, FunctionType.GetParameterTypes().Num()));
		}

		TEST_METHOD(QualTypeAcceptsOnlyGeneralUseQualifiers)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asCBuiltinType IntType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, PrimitiveUse(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32));
			AS_NAMESPACE_QUALIFIER asCQualType Legal;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(
				&IntType,
				AS_NAMESPACE_QUALIFIER asETypeQualifiers::Const | AS_NAMESPACE_QUALIFIER asETypeQualifiers::Reference,
				Legal)));
			ASSERT_THAT(IsTrue(Legal.IsConst()));
			ASSERT_THAT(IsTrue(Legal.IsReference()));

			AS_NAMESPACE_QUALIFIER asCQualType Invalid;
			ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCQualType::TryCreateRaw(&IntType, 0x80, Invalid)));
			ASSERT_THAT(IsFalse(Invalid.IsValid()));
		}

		TEST_METHOD(AuthoredTypeLocationIsSeparateFromCanonicalType)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			AS_NAMESPACE_QUALIFIER asCBuiltinType IntType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, PrimitiveUse(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32));
			AS_NAMESPACE_QUALIFIER asCQualType Use;
			AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(&IntType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, Use);
			AS_NAMESPACE_QUALIFIER asCTypeLoc AliasSpelling(Use, TEXT("Score"), Range(2, 7));
			AS_NAMESPACE_QUALIFIER asCTypeLoc CanonicalSpelling(Use, TEXT("int"), Range(12, 15));

			ASSERT_THAT(IsTrue(AliasSpelling.GetQualType() == CanonicalSpelling.GetQualType()));
			ASSERT_THAT(IsFalse(AliasSpelling.GetSpelling() == CanonicalSpelling.GetSpelling()));
			ASSERT_THAT(IsFalse(AliasSpelling.GetSourceRange() == CanonicalSpelling.GetSourceRange()));
		}

		TEST_METHOD(OrderedGenericArgumentsRemainStructural)
		{
			AS_NAMESPACE_QUALIFIER asCStableIdentityRegistry Registry;
			const auto IntKey = PrimitiveUse(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32);
			const auto BoolKey = PrimitiveUse(Registry, AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool);
			AS_NAMESPACE_QUALIFIER asCBuiltinType IntType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32, IntKey);
			AS_NAMESPACE_QUALIFIER asCBuiltinType BoolType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool, BoolKey);
			AS_NAMESPACE_QUALIFIER asCQualType IntUse;
			AS_NAMESPACE_QUALIFIER asCQualType BoolUse;
			AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(&IntType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, IntUse);
			AS_NAMESPACE_QUALIFIER asCQualType::TryCreate(&BoolType, AS_NAMESPACE_QUALIFIER asETypeQualifiers::None, BoolUse);
			const auto PairDecl = DeclKey(Registry, TEXT("Pair"), 2);
			AS_NAMESPACE_QUALIFIER asCNominalType PairType(PairDecl, NominalUse(Registry, PairDecl));
			AS_NAMESPACE_QUALIFIER asCGenericType First(&PairType, {IntUse, BoolUse}, NominalUse(Registry, PairDecl, {IntKey, BoolKey}));
			AS_NAMESPACE_QUALIFIER asCGenericType Second(&PairType, {BoolUse, IntUse}, NominalUse(Registry, PairDecl, {BoolKey, IntKey}));
			ASSERT_THAT(IsTrue(First.GetArguments()[0].GetType() == &IntType));
			ASSERT_THAT(IsTrue(Second.GetArguments()[0].GetType() == &BoolType));
			ASSERT_THAT(IsFalse(First.GetStableIdentity().IsZero()));
			ASSERT_THAT(IsTrue(First.GetStableIdentity() != Second.GetStableIdentity()));
		}

		TEST_METHOD(TypedAttributesPreserveArgumentsRangeAndTargets)
		{
			AS_NAMESPACE_QUALIFIER asCStringAttr Category(TEXT("Category"), TEXT("Gameplay"), Range(1, 20), AS_NAMESPACE_QUALIFIER asEAttrTarget::Function);
			AS_NAMESPACE_QUALIFIER asCFunctionDecl Function(TEXT("Run"), Range(0, 30));
			AS_NAMESPACE_QUALIFIER asCVarDecl Variable(AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Count"), Range(40, 50));
			ASSERT_THAT(IsTrue(Function.AddAttr(&Category)));
			ASSERT_THAT(IsFalse(Variable.AddAttr(&Category)));
			ASSERT_THAT(AreEqual(FString(TEXT("Gameplay")), Category.GetValue()));
			ASSERT_THAT(AreEqual(1, Function.GetAttrs().Num()));
		}

		TEST_METHOD(ErrorTypeIsExplicitAndNeverAStableIdentity)
		{
			AS_NAMESPACE_QUALIFIER asCErrorType Error;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Error, Error.GetKind()));
			ASSERT_THAT(IsFalse(Error.HasStableIdentity()));
			ASSERT_THAT(IsTrue(Error.IsInvalid()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
