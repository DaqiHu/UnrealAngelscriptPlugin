#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "frontend/AST/as_ast_context.h"
#include "frontend/AST/as_ast_verifier.h"
#include "frontend/AST/as_ast_visitor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTControlFlowTests
{
	static AS_NAMESPACE_QUALIFIER asCSourceManager ControlSnapshot()
	{
		const uint8 Bytes[] =
		{
			's','w','i','t','c','h','(','x',')','{','c','a','s','e',' ','1',':','f','a','l','l','t','h','r','o','u','g','h',';','d','e','f','a','u','l','t',':','b','r','e','a','k',';','}'
		};
		return AngelscriptNativeEngineTest::MakeSourceManagerFromBytes(TEXT("Control.as"), MakeArrayView(Bytes));
	}

	static AS_NAMESPACE_QUALIFIER asCSourceRange ControlRange(
		const AS_NAMESPACE_QUALIFIER asCSourceManager& Source,
		asUINT Begin,
		asUINT End)
	{
		return Source.MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin, End);
	}

	static AS_NAMESPACE_QUALIFIER asCIntegerLiteral* TypedInteger(
		AS_NAMESPACE_QUALIFIER asCASTContext& Context,
		const AS_NAMESPACE_QUALIFIER asCSourceManager& Source,
		int64 Value,
		asUINT Begin,
		asUINT End)
	{
		auto* Literal = Context.Create<AS_NAMESPACE_QUALIFIER asCIntegerLiteral>(
			Value, ControlRange(Source, Begin, End));
		Literal->SetExpressionSemantics(
			Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32),
			AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
		return Literal;
	}

	TEST_CLASS_WITH_FLAGS(
		ASTControlFlow,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(SwitchOwnsAuthoredClausesWhileTargetsRemainReferences)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Selector = TypedInteger(Context, Source, 0, 7, 8);
			auto* Switch = Context.Create<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(
				Selector, ControlRange(Source, 0, 9), ControlRange(Source, 0, 44));
			auto* Label = TypedInteger(Context, Source, 1, 15, 16);
			auto* Case = Context.Create<AS_NAMESPACE_QUALIFIER asCCaseStmt>(
				Switch, Label, ControlRange(Source, 10, 14), ControlRange(Source, 16, 17),
				ControlRange(Source, 10, 29));
			ASSERT_THAT(IsTrue(Case->SetResolvedValueBits(1)));
			auto* Default = Context.Create<AS_NAMESPACE_QUALIFIER asCDefaultStmt>(
				Switch, ControlRange(Source, 29, 36), ControlRange(Source, 36, 37),
				ControlRange(Source, 29, 43));
			auto* Fallthrough = Context.Create<AS_NAMESPACE_QUALIFIER asCFallthroughStmt>(
				nullptr, ControlRange(Source, 17, 29));
			ASSERT_THAT(IsTrue(Fallthrough->SetTarget(Default)));
			ASSERT_THAT(IsTrue(Case->AddStatement(Fallthrough)));
			ASSERT_THAT(IsTrue(Default->AddStatement(
				Context.Create<AS_NAMESPACE_QUALIFIER asCBreakStmt>(Switch, ControlRange(Source, 37, 43)))));
			ASSERT_THAT(IsTrue(Switch->AddClause(Case)));
			ASSERT_THAT(IsTrue(Switch->AddClause(Default)));

			TArray<const AS_NAMESPACE_QUALIFIER asCASTNode*> Children;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*Switch, Children)));
			ASSERT_THAT(AreEqual(3, Children.Num()));
			ASSERT_THAT(IsTrue(Children[0] == Selector));
			ASSERT_THAT(IsTrue(Children[1] == Case));
			ASSERT_THAT(IsTrue(Children[2] == Default));

			TArray<AS_NAMESPACE_QUALIFIER asSASTCrossReference> References;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectCrossReferences(*Case, References)));
			ASSERT_THAT(AreEqual(1, References.Num()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::SwitchParent, References[0].Kind));
			ASSERT_THAT(IsTrue(References[0].Target == Switch));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectCrossReferences(*Fallthrough, References)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTCrossReferenceKind::FallthroughTarget, References[0].Kind));
			ASSERT_THAT(IsTrue(References[0].Target == Default));

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsTrue(Context.VerifyAndSeal(Switch, Result)));
		}

		TEST_METHOD(ContinueCannotTargetSwitch)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Switch = Context.Create<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(
				TypedInteger(Context, Source, 0, 7, 8), ControlRange(Source, 0, 9),
				ControlRange(Source, 0, 44));
			auto* Case = Context.Create<AS_NAMESPACE_QUALIFIER asCCaseStmt>(
				Switch, TypedInteger(Context, Source, 1, 15, 16), ControlRange(Source, 10, 14),
				ControlRange(Source, 16, 17), ControlRange(Source, 10, 43));
			ASSERT_THAT(IsTrue(Case->SetResolvedValueBits(1)));
			ASSERT_THAT(IsTrue(Case->AddStatement(
				Context.Create<AS_NAMESPACE_QUALIFIER asCContinueStmt>(Switch, ControlRange(Source, 37, 43)))));
			ASSERT_THAT(IsTrue(Switch->AddClause(Case)));

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Switch, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidCrossReference, Result.Status));
		}

		TEST_METHOD(MultiForOwnsEachPhaseInAuthoredOrder)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* InitA = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(ControlRange(Source, 0, 1));
			auto* InitB = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(ControlRange(Source, 1, 2));
			auto* Condition = Context.Create<AS_NAMESPACE_QUALIFIER asCBooleanLiteral>(true, ControlRange(Source, 2, 3));
			Condition->SetExpressionSemantics(
				Context.GetBuiltinType(AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool), AS_NAMESPACE_QUALIFIER asEValueCategory::PRValue);
			auto* IncrementA = TypedInteger(Context, Source, 1, 3, 4);
			auto* IncrementB = TypedInteger(Context, Source, 2, 4, 5);
			auto* Body = Context.Create<AS_NAMESPACE_QUALIFIER asCNullStmt>(ControlRange(Source, 5, 6));
			auto* For = Context.Create<AS_NAMESPACE_QUALIFIER asCForStmt>(
				TArray<AS_NAMESPACE_QUALIFIER asCStmt*>{InitA, InitB}, Condition,
				TArray<AS_NAMESPACE_QUALIFIER asCExpr*>{IncrementA, IncrementB}, Body,
				ControlRange(Source, 0, 5), ControlRange(Source, 0, 6));

			TArray<const AS_NAMESPACE_QUALIFIER asCASTNode*> Children;
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCASTVisitor::CollectOwningChildren(*For, Children)));
			ASSERT_THAT(AreEqual(6, Children.Num()));
			ASSERT_THAT(IsTrue(Children[0] == InitA));
			ASSERT_THAT(IsTrue(Children[1] == InitB));
			ASSERT_THAT(IsTrue(Children[2] == Condition));
			ASSERT_THAT(IsTrue(Children[3] == IncrementA));
			ASSERT_THAT(IsTrue(Children[4] == IncrementB));
			ASSERT_THAT(IsTrue(Children[5] == Body));
		}

		TEST_METHOD(FallthroughMustTargetTheImmediateNextClause)
		{
			auto Source = ControlSnapshot();
			AS_NAMESPACE_QUALIFIER asCASTContext Context(Source);
			auto* Switch = Context.Create<AS_NAMESPACE_QUALIFIER asCSwitchStmt>(
				TypedInteger(Context, Source, 0, 7, 8), ControlRange(Source, 0, 9),
				ControlRange(Source, 0, 44));
			auto* First = Context.Create<AS_NAMESPACE_QUALIFIER asCCaseStmt>(
				Switch, TypedInteger(Context, Source, 1, 15, 16), ControlRange(Source, 10, 14),
				ControlRange(Source, 16, 17), ControlRange(Source, 10, 20));
			auto* Second = Context.Create<AS_NAMESPACE_QUALIFIER asCCaseStmt>(
				Switch, TypedInteger(Context, Source, 2, 15, 16), ControlRange(Source, 10, 14),
				ControlRange(Source, 16, 17), ControlRange(Source, 20, 30));
			auto* Third = Context.Create<AS_NAMESPACE_QUALIFIER asCDefaultStmt>(
				Switch, ControlRange(Source, 29, 36), ControlRange(Source, 36, 37),
				ControlRange(Source, 30, 43));
			First->SetResolvedValueBits(1);
			Second->SetResolvedValueBits(2);
			First->AddStatement(Context.Create<AS_NAMESPACE_QUALIFIER asCFallthroughStmt>(
				Third, ControlRange(Source, 17, 29)));
			Switch->AddClause(First);
			Switch->AddClause(Second);
			Switch->AddClause(Third);

			AS_NAMESPACE_QUALIFIER asSASTVerificationResult Result;
			ASSERT_THAT(IsFalse(Context.VerifyAndSeal(Switch, Result)));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEASTVerificationStatus::InvalidCrossReference, Result.Status));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
