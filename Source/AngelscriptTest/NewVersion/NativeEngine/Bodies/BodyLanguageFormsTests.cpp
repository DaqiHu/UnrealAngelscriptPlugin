#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace BodyLanguageFormsTests
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	TEST_CLASS_WITH_FLAGS(BodiesLanguageForms, "Angelscript.UnitTest.NativeEngine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ImplicitFieldReceiverIsARealThisExpression)
		{
			FSessionRun Run({{TEXT("This.as"), TEXT("class Item { int Value; int Read() { return Value; } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Record = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCRecordDecl>(Run.Session->Lookup(TEXT("Item"))[0]);
			auto* Method = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Record->Lookup(TEXT("Read"))[0]);
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Method->GetBody());
			auto* Value = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0])->GetValue();
			auto* Member = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCMemberExpr>(Value);
			ASSERT_THAT(IsNotNull(Member));
			auto* This = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCThisExpr>(Member->GetBase());
			ASSERT_THAT(IsNotNull(This));
			ASSERT_THAT(IsTrue(This->IsImplicit()));
			ASSERT_THAT(IsTrue(This->GetRecord() == Record));
			ASSERT_THAT(IsTrue(This->GetExpressionQualType().IsHandle()));
			ASSERT_THAT(IsTrue(This->GetValueCategory() == AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(ConstMethodCannotMutateImplicitReceiver)
		{
			for (const TCHAR* Operation : {TEXT("Value = 1"), TEXT("this.Value = 1"), TEXT("Mutate()")})
			{
				FSessionRun Run({{TEXT("ConstThis.as"), FString::Printf(TEXT("class Item { int Value; void Mutate(); void Read() const { %s; } }"), Operation)}});
				ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
				ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
				ASSERT_THAT(IsFalse(Run.Session->VerifyAST()));
			}
		}

		TEST_METHOD(PrivateMemberIsReadableWithinOwnerButNotOutside)
		{
			FSessionRun Owner({{TEXT("PrivateOwner.as"), TEXT("class Item { private int Value; int Read() { return Value; } }")}});
			ASSERT_THAT(IsTrue(Owner.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Owner.Session->AnalyzeBodies(1)));
			const bool Verified = Owner.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Owner.Session->GetVerificationError()));
			FSessionRun Outside({{TEXT("PrivateOutside.as"), TEXT("class Item { private int Value; } int F(Item Obj) { return Obj.Value; }")}});
			ASSERT_THAT(IsTrue(Outside.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Outside.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(ProtectedMemberIsVisibleThroughTheDerivedLexicalOwner)
		{
			FSessionRun Run({{TEXT("Protected.as"), TEXT("class Base { protected int Value; } class Derived : Base { int Read() { return Value; } }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
		}

		TEST_METHOD(MultipleDeclaratorsRemainInTheSurroundingScope)
		{
			FSessionRun Run({{TEXT("Multiple.as"), TEXT("int F() { int First = 1, Second = First + 2; return Second; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Group = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclGroupStmt>(Body->GetStatements()[0]);
			ASSERT_THAT(IsNotNull(Group));
			ASSERT_THAT(AreEqual(2, Group->GetDeclarations().Num()));
			auto* Return = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[1]);
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Return->GetValue())->GetDecl() == AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCDeclStmt>(Group->GetDeclarations()[1])->GetDeclaration()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(MixinMemberSyntaxCallsTheActualGlobalFunctionWithReceiver)
		{
			FSessionRun Run({{TEXT("Mixin.as"), TEXT("class Item { int Value; } mixin int Plus(const Item&in Self, int Amount) { return Self.Value + Amount; } int F(Item Obj) { return Obj.Plus(2); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			auto* Body = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
			auto* Call = AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCCallExpr>(AS_NAMESPACE_QUALIFIER cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements()[0])->GetValue());
			auto* Callee = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Call->GetCallee());
			ASSERT_THAT(IsNotNull(Callee));
			ASSERT_THAT(IsTrue(Callee->GetDecl() == Run.Function(TEXT("Plus"))));
			ASSERT_THAT(AreEqual(2, Call->GetArguments().Num()));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}

		TEST_METHOD(ImplicitMixinReceiverPreservesConstObjectBinding)
		{
			FSessionRun Run({{TEXT("ImplicitMixin.as"), TEXT("class Item { int Value; int Read() const { return Plus(2); } } mixin int Plus(const Item&in Self, int Amount) { return Self.Value + Amount; }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			const bool Verified = Run.Session->VerifyAST();
			ASSERT_THAT(IsTrue(Verified, Run.Session->GetVerificationError()));
			FSessionRun Invalid({{TEXT("ConstMixin.as"), TEXT("class Item { int Value; int Read() const { return Plus(2); } } mixin int Plus(Item&inout Self, int Amount) { Self.Value += Amount; return Self.Value; }")}});
			ASSERT_THAT(IsTrue(Invalid.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Invalid.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(MixinIsNotAnOrdinaryGlobalOverload)
		{
			FSessionRun Run({{TEXT("MixinGlobal.as"), TEXT("class Item {} mixin int Plus(const Item&in Self) { return 1; } int F(Item Obj) { return Plus(Obj); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		}

		TEST_METHOD(NoDiscardWarnsOnlyWhenTheCallResultIsDiscarded)
		{
			FSessionRun Discarded({{TEXT("Discarded.as"), TEXT("int Read() no_discard { return 1; } void F() { Read(); }")}});
			ASSERT_THAT(IsTrue(Discarded.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Discarded.Session->AnalyzeBodies(1)));
			AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Diagnostics;
			Discarded.Diagnostics.EmitTo(Diagnostics);
			ASSERT_THAT(AreEqual(1, Diagnostics.GetRecords().Num()));
			ASSERT_THAT(IsTrue(Diagnostics.GetRecords()[0].Severity == AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Warning));
			FSessionRun Used({{TEXT("Used.as"), TEXT("int Read() no_discard { return 1; } int F() { return Read(); }")}});
			ASSERT_THAT(IsTrue(Used.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Used.Session->AnalyzeBodies(1)));
			ASSERT_THAT(AreEqual(asUINT(0), Used.Diagnostics.GetPendingCount()));
		}

		TEST_METHOD(DeprecatedCallReportsAWarningWithoutDiscardingTheTypedResult)
		{
			FSessionRun Run({{TEXT("Deprecated.as"), TEXT("int Old() deprecated { return 1; } int F() { return Old(); }")}});
			ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
			ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
			AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Diagnostics;
			Run.Diagnostics.EmitTo(Diagnostics);
			ASSERT_THAT(AreEqual(1, Diagnostics.GetRecords().Num()));
			ASSERT_THAT(IsTrue(Diagnostics.GetRecords()[0].Severity == AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Warning));
			ASSERT_THAT(IsTrue(Run.Session->VerifyAST()));
		}
	};
}

#endif
