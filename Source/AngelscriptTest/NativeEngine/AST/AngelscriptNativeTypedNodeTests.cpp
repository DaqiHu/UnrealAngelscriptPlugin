#include "CQTest.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"
#include "frontend/AST/as_stmt.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace TypedASTNodeTests
{
	static AS_NAMESPACE_QUALIFIER asCSourceRange Range(asUINT Begin, asUINT End)
	{
		return AS_NAMESPACE_QUALIFIER asCSourceRange(
			AS_NAMESPACE_QUALIFIER asCSourceLocation(7, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), Begin),
			AS_NAMESPACE_QUALIFIER asCSourceLocation(7, AS_NAMESPACE_QUALIFIER asCSourceFileID(1), End));
	}

	TEST_CLASS_WITH_FLAGS(
		ASTTypedNodes,
		"Angelscript.UnitTest.NativeEngine.AST",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(ConcreteDeclsExposeOnlyTheirPayload)
		{
			AS_NAMESPACE_QUALIFIER asCFunctionDecl Function(TEXT("Compute"), Range(10, 30));
			AS_NAMESPACE_QUALIFIER asCVarDecl Parameter(AS_NAMESPACE_QUALIFIER asEDeclKind::ParmVar, TEXT("Value"), Range(18, 23));
			Function.AddParameter(&Parameter);
			Function.SetDeferredBodyRange(Range(24, 30));

			ASSERT_THAT(AreEqual(FString(TEXT("Compute")), Function.GetName()));
			ASSERT_THAT(AreEqual(1, Function.GetParameters().Num()));
			ASSERT_THAT(IsTrue(Function.HasDeferredBody()));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDeclKind::Function, Function.GetKind()));
		}

		TEST_METHOD(DeclContextIsOrthogonalToDeclKind)
		{
			AS_NAMESPACE_QUALIFIER asCNamespaceDecl Namespace(TEXT("Gameplay"), Range(0, 40));
			AS_NAMESPACE_QUALIFIER asCRecordDecl Record(TEXT("Actor"), Range(5, 35));
			AS_NAMESPACE_QUALIFIER asCVarDecl Field(AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Health"), Range(12, 20));
			Namespace.AddDecl(&Record);
			Record.AddDecl(&Field);

			ASSERT_THAT(AreEqual(1, Namespace.GetDecls().Num()));
			ASSERT_THAT(AreEqual(1, Record.Lookup(TEXT("Health")).Num()));
			ASSERT_THAT(IsTrue(Field.GetDeclContext() == &Record));
		}

		TEST_METHOD(ExpressionsFollowValueStatementLineage)
		{
			static_assert(std::is_base_of_v<AS_NAMESPACE_QUALIFIER asCStmt, AS_NAMESPACE_QUALIFIER asCValueStmt>);
			static_assert(std::is_base_of_v<AS_NAMESPACE_QUALIFIER asCValueStmt, AS_NAMESPACE_QUALIFIER asCExpr>);
			static_assert(std::is_base_of_v<AS_NAMESPACE_QUALIFIER asCExpr, AS_NAMESPACE_QUALIFIER asCIntegerLiteral>);
			AS_NAMESPACE_QUALIFIER asCIntegerLiteral Literal(42, Range(1, 3));
			AS_NAMESPACE_QUALIFIER asCStmt* Statement = &Literal;
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEStmtKind::IntegerLiteral, Statement->GetKind()));
			ASSERT_THAT(AreEqual(static_cast<int64>(42), Literal.GetValue()));
		}

		TEST_METHOD(ConcreteStatementsRetainOrderedChildren)
		{
			AS_NAMESPACE_QUALIFIER asCIntegerLiteral Condition(1, Range(3, 4));
			AS_NAMESPACE_QUALIFIER asCReturnStmt ThenBranch(&Condition, Range(5, 12));
			AS_NAMESPACE_QUALIFIER asCIfStmt If(&Condition, &ThenBranch, nullptr, Range(0, 12));
			ASSERT_THAT(IsTrue(If.GetCondition() == &Condition));
			ASSERT_THAT(IsTrue(If.GetThen() == &ThenBranch));
			ASSERT_THAT(IsTrue(If.GetElse() == nullptr));
		}

		TEST_METHOD(CheckedCastsRejectWrongFamiliesAndKinds)
		{
			AS_NAMESPACE_QUALIFIER asCVarDecl Variable(AS_NAMESPACE_QUALIFIER asEDeclKind::Var, TEXT("Count"), Range(0, 5));
			AS_NAMESPACE_QUALIFIER asCIntegerLiteral Literal(3, Range(6, 7));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER isa<AS_NAMESPACE_QUALIFIER asCVarDecl>(&Variable)));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(&Variable) == nullptr));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDecl>(&Literal) == nullptr));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCExpr>(&Literal) == &Literal));
		}

		TEST_METHOD(RepresentativeNodesDoNotCarryUniversalWidePayload)
		{
			ASSERT_THAT(IsTrue(sizeof(AS_NAMESPACE_QUALIFIER asCIntegerLiteral) <= 64));
			ASSERT_THAT(IsTrue(sizeof(AS_NAMESPACE_QUALIFIER asCReturnStmt) <= 64));
			ASSERT_THAT(IsTrue(sizeof(AS_NAMESPACE_QUALIFIER asCVarDecl) <= 160));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
