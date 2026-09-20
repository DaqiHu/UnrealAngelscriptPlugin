#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Parser/as_parser.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ParserPrecedence, "Angelscript.UnitTest.NativeEngine.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static AS_NAMESPACE_QUALIFIER asCExpr* ReturnExpr(const FSessionRun& Run)
	{
		auto* Body = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCCompoundStmt>(Run.Function(TEXT("F"))->GetBody());
		auto* Return = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCReturnStmt>(Body->GetStatements().Last());
		return Return ? Return->GetValue() : nullptr;
	}

	TEST_METHOD(AddRootHasMultiplyOnTheRight)
	{
		const FString Source = TEXT("int F(int A){ return A + 2 * 3; }");
		FSessionRun Run({{TEXT("AddMul.as"), Source}});
		asCParser Parser(Run.Session->GetSema());
		(void)Parser;
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		auto* Add = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnExpr(Run));
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Add, Add->GetOperator()));
		auto* Multiply = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Add->GetRight());
		ASSERT_THAT(IsNotNull(Multiply));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Multiply, Multiply->GetOperator()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Source.Find(TEXT("+"))), Add->GetOperatorRange().Begin.ByteOffset));
		ASSERT_THAT(AreEqual(asUINT(1), Add->GetOperatorRange().Length()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Source.Find(TEXT("*"))), Multiply->GetOperatorRange().Begin.ByteOffset));
		ASSERT_THAT(AreEqual(asUINT(1), Multiply->GetOperatorRange().Length()));
		ASSERT_THAT(IsNotNull(AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCDeclRefExpr>(Add->GetLeft())));
	}

	TEST_METHOD(ParenthesizedAddHasMultiplyRoot)
	{
		const FString Source = TEXT("int F(int A){ return (A + 2) * 3; }");
		FSessionRun Run({{TEXT("ParenMul.as"), Source}});
		asCParser Parser(Run.Session->GetSema());
		(void)Parser;
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		auto* Multiply = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(ReturnExpr(Run));
		ASSERT_THAT(IsNotNull(Multiply));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Multiply, Multiply->GetOperator()));
		auto* Paren = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCParenExpr>(Multiply->GetLeft());
		ASSERT_THAT(IsNotNull(Paren));
		auto* Add = AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCBinaryOperator>(Paren->GetSubExpr());
		ASSERT_THAT(IsNotNull(Add));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBinaryOperator::Add, Add->GetOperator()));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Source.Find(TEXT("*"))), Multiply->GetOperatorRange().Begin.ByteOffset));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Source.Find(TEXT("+"))), Add->GetOperatorRange().Begin.ByteOffset));
	}
};

#endif
