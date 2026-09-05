#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/as_ast_cast.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
namespace BodyAccessTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;
	TEST_CLASS_WITH_FLAGS(BodiesAccess, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(NamedGlobalCallerReceivesReadAndWritePermission)
		{
			FSessionRun Run({{TEXT("NamedAccess.as"), TEXT("class Item { access Friends = private, Edit; access:Friends int Value; } int Edit(Item Obj) { Obj.Value = 42; return Obj.Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(UnlistedCallerCannotReadAndLaterStatementSurvives)
		{
			FSessionRun Run({{TEXT("DeniedAccess.as"), TEXT("class Item { access Friends = private, Allowed; access:Friends int Value; } int F(Item Obj) { Obj.Value; return 9; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
			ASSERT_THAT(IsTrue(Run.Session->SerializeStableBodyDiagnostics().Contains(TEXT("inaccessible-member"))));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			ASSERT_THAT(AreEqual(9ll, AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements().Last())->GetValue())->GetValue()));
		}

		TEST_METHOD(WildcardReadonlyPreservesReadableLValueButForbidsMutation)
		{
			FSessionRun Read({{TEXT("ReadAccess.as"), TEXT("class Item { access View = private, *(readonly); access:View int Value; } int F(Item Obj) { return Obj.Value; }")}});
			ASSERT_THAT(IsTrue(Read.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Read.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Read.Function(TEXT("F"))->GetBody());
			auto* Member = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0])->GetValue());
			ASSERT_THAT(IsTrue(Member->IsReadOnly()));
			ASSERT_THAT(IsTrue(Member->GetValueCategory() == AS_NAMESPACE_QUALIFIER asEValueCategory::LValue));
			const bool Verified = Read.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Read.Session->GetVerificationError()));
			for (const TCHAR* Write : {TEXT("Obj.Value = 1"), TEXT("Obj.Value += 1"), TEXT("++Obj.Value"), TEXT("(Obj.Value) = 1")})
			{
				FSessionRun Denied({{TEXT("WriteDenied.as"), FString::Printf(TEXT("class Item { access View = private, *(readonly); access:View int Value; } void F(Item Obj) { %s; }"), Write)}});
				ASSERT_THAT(IsTrue(Denied.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Denied.Session->AnalyzeBodies(1), Write));
			}
		}

		TEST_METHOD(WildcardWithoutModifiersDoesNotAccidentallyGrantPublicAccess)
		{
			FSessionRun Run({{TEXT("BareWildcard.as"), TEXT("class Item { access Nothing = private, *; access:Nothing int Value; } int F(Item Obj) { return Obj.Value; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(OwnerAlwaysRetainsFullAccessToItsOwnPolicy)
		{
			FSessionRun Run({{TEXT("OwnerAccess.as"), TEXT("class Item { access View = private, *(readonly); access:View int Value; int F() { Value = 42; return Value; } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ProtectedPolicyGrantsDerivedCallerButNotUnrelatedCaller)
		{
			FSessionRun Derived({{TEXT("ProtectedPolicy.as"), TEXT("class Base { access Family = protected; access:Family int Value; } class Child : Base { int Read() { Value = 42; return Value; } }")}});
			ASSERT_THAT(IsTrue(Derived.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Derived.Session->AnalyzeBodies(1)));
			const bool Verified = Derived.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Derived.Session->GetVerificationError()));
			FSessionRun Other({{TEXT("UnrelatedPolicy.as"), TEXT("class Base { access Family = protected; access:Family int Value; } int F(Base Obj) { return Obj.Value; }")}});
			ASSERT_THAT(IsTrue(Other.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Other.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(NamedCallerTypeAndNamespaceAreIndependentPermissionSubjects)
		{
			for (const TCHAR* Subject : {TEXT("Reader"), TEXT("Tools")})
			{
				FSessionRun Run({{TEXT("NamedSubject.as"), FString::Printf(TEXT("class Item { access Friends = private, %s; access:Friends int Value; } namespace Tools { class Reader { int F(Item Obj) { return Obj.Value; } } }"), Subject)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1), Subject));
				const bool Verified = Run.Session->VerifyAST();
				ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			}
		}

		TEST_METHOD(InheritedSubjectPermissionFollowsCallerBasesOnlyWhenRequested)
		{
			for (const bool Inherited : {false, true})
			{
				FSessionRun Run({{TEXT("InheritedSubject.as"), FString::Printf(TEXT("class Item { access Friends = private, Reader%s; access:Friends int Value; } class Reader {} class Child : Reader { int F(Item Obj) { return Obj.Value; } }"), Inherited ? TEXT("(inherited)") : TEXT(""))}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(AreEqual(Inherited, Run.Session->AnalyzeBodies(1)));
				if (Inherited) { const bool Verified = Run.Session->VerifyAST(); ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError())); }
			}
		}

		TEST_METHOD(ReadonlyMethodPolicyAllowsConstCallsButNotMutableCalls)
		{
			FSessionRun Read({{TEXT("ReadonlyMethod.as"), TEXT("class Item { access View = private, *(readonly); access:View int Read() const; access:View void Mutate(); } int F(Item Obj) { return Obj.Read(); }")}});
			ASSERT_THAT(IsTrue(Read.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Read.Session->AnalyzeBodies(1)));
			const bool Verified = Read.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Read.Session->GetVerificationError()));
			FSessionRun Write({{TEXT("ReadonlyMethodDenied.as"), TEXT("class Item { access View = private, *(readonly); access:View void Mutate(); } void F(Item Obj) { Obj.Mutate(); }")}});
			ASSERT_THAT(IsTrue(Write.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Write.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(EditDefaultsOnlyPermissionRequiresAnActualDefaultsContext)
		{
			for (const bool Defaults : {false, true})
			{
				FSessionRun Run({{TEXT("EditDefaults.as"), FString::Printf(TEXT("class Item { access Edit = private, F(editdefaults); access:Edit int Value; } int F(Item Obj)%s { Obj.Value = 42; return Obj.Value; }"), Defaults ? TEXT(" defaults") : TEXT(""))}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(AreEqual(Defaults, Run.Session->AnalyzeBodies(1)));
				if (Defaults) { const bool Verified = Run.Session->VerifyAST(); ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError())); }
			}
		}

		TEST_METHOD(RepeatedWildcardPermissionsUnionReadAndEditWithoutGrantingOrdinaryWrite)
		{
			const FString Definitions = TEXT("class Item { access Edit = private, *(readonly), *(editdefaults, inherited); access:Edit int Value; } ");
			FSessionRun Defaults({{TEXT("WildcardUnion.as"), Definitions + TEXT("void F(Item Obj) defaults { Obj.Value = 42; }")}});
			ASSERT_THAT(IsTrue(Defaults.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Defaults.Session->AnalyzeBodies(1)));
			const bool Verified = Defaults.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Defaults.Session->GetVerificationError()));
			FSessionRun Ordinary({{TEXT("WildcardUnionDenied.as"), Definitions + TEXT("void F(Item Obj) { Obj.Value = 42; }")}});
			ASSERT_THAT(IsTrue(Ordinary.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Ordinary.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(OrdinaryConstructorDoesNotAcquireEditDefaultsPermission)
		{
			FSessionRun Run({{TEXT("ConstructorAccess.as"), TEXT("class Item { access Edit = private, *(editdefaults); access:Edit int Value; } class User { User(Item Obj) { Obj.Value = 42; } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(EditorOnlyAccessHonorsExplicitTypeContext)
		{
			const FString Script = TEXT("class Item { access Edit = private, *(editdefaults); access:Edit int Value; } void Editor_Paint(Item Obj) { Obj.Value = 42; }");
			for (const bool AllowEditor : {false, true})
			{
				AS_NAMESPACE_QUALIFIER asSTypeContextOptions Options;
				Options.AllowEditorOnlyAccess = AllowEditor;
				auto Context = MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>(Options);
				FSessionRun Run({{TEXT("EditorAccess.as"), Script}}, {}, Context);
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(AreEqual(AllowEditor, Run.Session->AnalyzeBodies(1)));
				if (AllowEditor)
				{
					const bool Verified = Run.Session->VerifyAST();
					ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
				}
			}
		}

		TEST_METHOD(DefaultsOnlyAndConstructionUnsafeCallsHaveDistinctBoundaries)
		{
			for (const TCHAR* Script : {
				TEXT("void Edit() defaults; void F() { Edit(); }"),
				TEXT("void Unsafe() unsafe_during_construction; class Item { Item() { Unsafe(); } }"),
				TEXT("void Unsafe() unsafe_during_construction; class Item { default Unsafe(); }")})
			{
				FSessionRun Run({{TEXT("ContextCall.as"), Script}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1), Script));
			}
		}
	};
}
#endif
