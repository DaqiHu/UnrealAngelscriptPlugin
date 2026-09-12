#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DeclarationLanguageTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	TEST_CLASS_WITH_FLAGS(DeclarationsExpandedLanguage, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(EnumInitializersRemainDeferredUntilResolution)
		{
			FSessionRun Run({{TEXT("EnumSource.as"), TEXT("enum Flags { A = 1 << 2, B }")}});
			ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
			auto* Enum = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
			auto* First = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Enum->GetDecls()[0]);
			auto* Second = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Enum->GetDecls()[1]);
			ASSERT_THAT(IsTrue(First->HasExplicitValue()));
			ASSERT_THAT(IsTrue(First->HasDeferredInitializer()));
			ASSERT_THAT(IsFalse(First->HasResolvedValue()));
			ASSERT_THAT(IsNull(First->GetInitializer()));
			ASSERT_THAT(IsFalse(Second->HasResolvedValue()));
			ASSERT_THAT(IsFalse(Second->HasDeferredInitializer()));
			ASSERT_THAT(AreEqual(asUINT(6), First->GetDeferredInitializerRange().Length()));
		}

		TEST_METHOD(EnumArithmeticReferencesAndImplicitSequenceUseTypedExpressions)
		{
			FSessionRun Run({{TEXT("EnumValues.as"), TEXT("enum Flags { A = 1 << 2, B = A | 3, C = B + 1, D }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			auto* Enum = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Flags"))[0]);
			const int64 Expected[] = {4, 7, 8, 9};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
			{
				auto* Constant = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCEnumConstantDecl>(Enum->GetDecls()[Index]);
				ASSERT_THAT(IsTrue(Constant->HasResolvedValue()));
				ASSERT_THAT(AreEqual(Expected[Index], Constant->GetValue()));
				if (Index < 3) ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Constant->GetInitializer())));
			}
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(EnumOverflowAndInvalidExpressionPreventResolution)
		{
			for (const TCHAR* Expression : {TEXT("2147483647 + 1"), TEXT("1 / 0"), TEXT("Unknown + 1")})
			{
				FSessionRun Run({{TEXT("EnumInvalid.as"), FString::Printf(TEXT("enum Value { Bad = %s }"), Expression)}});
				ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			}
		}

		TEST_METHOD(ScopedEnumReferenceRetainsNominalTypeBeforeIntegralConversion)
		{
			FSessionRun Run({{TEXT("EnumReference.as"), TEXT("enum Flags { Enabled = 4 } int F() { return Flags::Enabled + 1; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Return = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0]);
			auto* Binary = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Return->GetValue());
			auto* Conversion = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCImplicitCastExpr>(Binary->GetLeft());
			ASSERT_THAT(IsNotNull(Conversion));
			auto* Reference = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Conversion->GetOperand());
			ASSERT_THAT(IsNotNull(Reference));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETypeKind::Nominal, Reference->GetExpressionType()->GetKind()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclKind::EnumConstant, Reference->GetDecl()->GetKind()));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(RemovedPropertySyntaxDoesNotPublishFunctionsOrEatLaterDeclarations)
		{
			for (const TCHAR* Removed : {TEXT("int Bad() property { return 1; }"), TEXT("int Bad { get { return 1; } }")})
			{
				FSessionRun Run({{TEXT("RemovedProperty.as"), FString::Printf(TEXT("class Item { %s int Later() { return 2; } }"), Removed)}});
				ASSERT_THAT(IsTrue(Run.Session->CloseDeclarationBarrier()));
				ASSERT_THAT(IsFalse(Run.Session->ResolveDeclarations(1)));
				auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
				ASSERT_THAT(AreEqual(0, Record->Lookup(TEXT("Bad")).Num()));
				ASSERT_THAT(AreEqual(1, Record->Lookup(TEXT("Later")).Num()));
			}
		}

		TEST_METHOD(MaintainedMethodSuffixesRemainTypedDefinitionFacts)
		{
			struct FSuffix { const TCHAR* Source; AS_NAMESPACE_QUALIFIER asEFunctionModifiers Flag; };
			const FSuffix Cases[] = {
				{TEXT("accept_temporary_this"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::AcceptTemporary},
				{TEXT("external_implicit_this"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::ExternalImplicitThis},
				{TEXT("no_discard"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::NoDiscard},
				{TEXT("allow_discard"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::AllowDiscard},
				{TEXT("__generated"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::Generated},
				{TEXT("deprecated"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::Deprecated},
				{TEXT("defaults"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::Defaults},
				{TEXT("unsafe_during_construction"), AS_NAMESPACE_QUALIFIER asEFunctionModifiers::UnsafeDuringConstruction},
			};
			for (const auto& Case : Cases)
			{
				FSessionRun Run({{TEXT("Suffix.as"), FString::Printf(TEXT("class Item { int Read() %s { return 1; } }"), Case.Source)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Item"))[0]);
				auto* Method = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Read"))[0]);
				ASSERT_THAT(IsTrue((static_cast<uint16>(Method->GetModifiers()) & static_cast<uint16>(Case.Flag)) != 0));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
			}
		}
	};
}

#endif
