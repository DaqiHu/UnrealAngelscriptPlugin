#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace DeclarationAccessTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(DeclarationsAccess, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(PolicyKeepsOrderedSubjectsAndExactAuthoredModifierRanges)
		{
			const FString Script = TEXT("class Item { access Friends = private, Reader(readonly, inherited), *(editdefaults, inherited); access:Friends int Value; }");
			FSessionRun Run({{TEXT("PolicySource.as"), Script}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Item"))[0]);
			auto* Policy = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCAccessSpecifierDecl>(Record->Lookup(TEXT("Friends"))[0]);
			ASSERT_THAT(IsNotNull(Policy));
			ASSERT_THAT(IsTrue(Policy->GetStableIdentity().IsZero()));
			ASSERT_THAT(IsTrue(Policy->GetDeclContext() == Record));
			ASSERT_THAT(AreEqual(2, Policy->GetPermissions().Num()));
			const auto& Named = Policy->GetPermissions()[0];
			const auto& Wildcard = Policy->GetPermissions()[1];
			ASSERT_THAT(AreEqual(FString(TEXT("Reader")), Named.Name));
			ASSERT_THAT(AreEqual(asUINT(Script.Find(TEXT("Reader"))), Named.SubjectRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(asUINT(6), Named.SubjectRange.Length()));
			ASSERT_THAT(AreEqual(2, Named.Modifiers.Num()));
			ASSERT_THAT(IsTrue(Named.Modifiers[0].Kind == AS_NAMESPACE_QUALIFIER asEAccessPermissionModifier::ReadOnly));
			ASSERT_THAT(AreEqual(asUINT(Script.Find(TEXT("readonly"))), Named.Modifiers[0].Range.Begin.ByteOffset));
			ASSERT_THAT(IsTrue(Wildcard.Subject == AS_NAMESPACE_QUALIFIER asEAccessPermissionSubject::Wildcard));
			ASSERT_THAT(IsTrue(Wildcard.Name.IsEmpty()));
			ASSERT_THAT(AreEqual(asUINT(1), Wildcard.SubjectRange.Length()));
			ASSERT_THAT(IsTrue(Wildcard.Modifiers[1].Kind == AS_NAMESPACE_QUALIFIER asEAccessPermissionModifier::Inherited));
			auto* Variable = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCVarDecl>(Record->Lookup(TEXT("Value"))[0]);
			ASSERT_THAT(IsNotNull(Variable->GetAuthoredAccessUse()));
			ASSERT_THAT(IsTrue(Variable->GetAuthoredAccessUse()->Resolved == Policy));
			ASSERT_THAT(AreEqual(asUINT(1), Variable->GetAuthoredAccessUse()->ColonRange.Length()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ForwardPolicyUseResolvesAfterTheDeclarationBarrier)
		{
			FSessionRun Run({{TEXT("ForwardPolicy.as"), TEXT("class Item { access:Friends int Value; access:Friends int Read() const; access Friends = private, *(readonly); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Item"))[0]);
			auto* Policy = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCAccessSpecifierDecl>(Record->Lookup(TEXT("Friends"))[0]);
			auto* Function = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Read"))[0]);
			ASSERT_THAT(IsNotNull(Function->GetAuthoredAccessUse()));
			ASSERT_THAT(IsTrue(Function->GetAuthoredAccessUse()->Resolved == Policy));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(MissingDuplicateAndForeignPoliciesCannotBecomeResolvedDefinitions)
		{
			for (const TCHAR* Script : {
				TEXT("class Item { access:Missing int Value; }"),
				TEXT("class Item { access Friends = private; access Friends = protected; access:Friends int Value; }"),
				TEXT("class Base { access Friends = protected; } class Item : Base { access:Friends int Value; }")})
			{
				FSessionRun Run({{TEXT("InvalidPolicyBinding.as"), Script}});
				ASSERT_THAT(IsFalse(Run.ResolveDeclarations(), Script));
				ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			}
		}

		TEST_METHOD(MalformedPolicyRecoversTheFollowingOrdinaryDeclaration)
		{
			for (const TCHAR* Policy : {
				TEXT("access Broken = public;"), TEXT("access Broken = private, Reader(unknown);"),
				TEXT("access Broken = private, ;"), TEXT("access:Broken private int Hidden;")})
			{
				FSessionRun Run({{TEXT("PolicyRecovery.as"), FString::Printf(TEXT("class Item { %s int Later; }"), Policy)}});
				ASSERT_THAT(IsFalse(Run.ResolveDeclarations(), Policy));
				auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
				ASSERT_THAT(AreEqual(1, Record->Lookup(TEXT("Later")).Num()));
			}
		}

		TEST_METHOD(PolicyDeclarationAndUseAreNotGlobalLanguageConstructs)
		{
			for (const TCHAR* Script : {TEXT("access Friends = private; int Later;"), TEXT("access:Friends int Hidden; int Later;")})
			{
				FSessionRun Run({{TEXT("GlobalPolicy.as"), Script}});
				ASSERT_THAT(IsFalse(Run.ResolveDeclarations()));
				ASSERT_THAT(AreEqual(1, Run.Session->GetTranslationUnit()->Lookup(TEXT("Later")).Num()));
			}
		}

		TEST_METHOD(ConversionDeclarationsRejectParametersAndVoidDestinations)
		{
			for (const TCHAR* Declaration : {TEXT("void opConv();"), TEXT("int opImplConv(int Extra);"), TEXT("void opCast();"), TEXT("int opImplCast(int Extra);")})
			{
				FSessionRun Run({{TEXT("InvalidConversionShape.as"), FString::Printf(TEXT("class Source { %s int Later(); }"), Declaration)}});
				ASSERT_THAT(IsFalse(Run.ResolveDeclarations(), Declaration));
				auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->GetTranslationUnit()->GetDecls()[0]);
				ASSERT_THAT(AreEqual(1, Record->Lookup(TEXT("Later")).Num()));
			}
		}
	};
}
#endif
