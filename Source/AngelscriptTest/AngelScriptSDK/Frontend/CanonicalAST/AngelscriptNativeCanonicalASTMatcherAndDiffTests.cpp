#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
enum class EMatcherFixtureDifference
{
	None,
	Type,
	Decl,
	Stmt,
	Expr,
};

struct FMatcherFixture
{
	asASTDeclId Tu;
	asASTDeclId Dtor;
	asASTDeclId Function;
	asASTStmtId Loop;
	asASTStmtId Continue;
	asASTStmtId ConversionStmt;
	asASTExprId Conversion;
	asASTExprId Receiver;
	asASTExprId Call;
	asASTExprId Cleanup;
};

FMatcherFixture BuildMatcherFixture(asCASTContext& Context, EMatcherFixtureDifference Difference)
{
	FMatcherFixture Fixture;
	const char Source[] = "int F() { while (true) { continue; } }";
	const asASTFileID File = Context.AddSourceSection(
		"Matcher/Fixture.as", asAST_SOURCE_AUTHORED, Source, sizeof(Source) - 1, 6);
	Fixture.Tu = Context.CreateTranslationUnit("MatcherFixture");
	const asCQualType ScalarType = Context.InternPrimitive(
		Difference == EMatcherFixtureDifference::Type ? ttUInt : ttInt, 0);
	Fixture.Dtor = Context.CreateDecl(asAST_DECL_DESTRUCTOR, Fixture.Tu, asCSourceRange(), "~T");
	Context.SetStableKey(Fixture.Dtor, "T::~T()");
	Fixture.Function = Context.CreateDecl(
		asAST_DECL_FUNCTION,
		Fixture.Tu,
		Context.GetSourceManager().MakeRange(File, 0, sizeof(Source) - 1),
		"F");
	Context.SetStableKey(Fixture.Function, "F()");
	Context.SetDeclType(Fixture.Function, ScalarType);
	if (Difference == EMatcherFixtureDifference::Decl)
	{
		Context.AddDeclTrait(Fixture.Function, asAST_TRAIT_GENERATED);
	}

	Fixture.Loop = Context.CreateStmt(
		asAST_STMT_WHILE, Fixture.Function, Context.GetSourceManager().MakeRange(File, 10, 40));
	Context.SetBody(Fixture.Function, Fixture.Loop);
	Fixture.Continue = Context.CreateStmt(
		asAST_STMT_CONTINUE, Fixture.Function, Context.GetSourceManager().MakeRange(File, 29, 38));
	Context.AddStmtChild(Fixture.Loop, Fixture.Continue);
	Context.SetTarget(Fixture.Continue, Fixture.Loop);

	Fixture.ConversionStmt = Context.CreateStmt(
		asAST_STMT_EXPR, Fixture.Function, Context.GetSourceManager().MakeRange(File, 4, 9));
	if (Difference == EMatcherFixtureDifference::Stmt)
	{
		Context.SetStmtSafePointRole(Fixture.ConversionStmt, asAST_SAFEPOINT_STATEMENT);
	}
	Context.AddStmtChild(Fixture.Loop, Fixture.ConversionStmt);
	const asASTExprId Literal = Context.CreateExpr(
		asAST_EXPR_INTEGER_LITERAL, ScalarType, asAST_VALUE_PRVALUE, Context.GetSourceManager().MakeRange(File, 4, 5));
	Context.SetLiteral(Literal, Difference == EMatcherFixtureDifference::Expr ? "2" : "1");
	Fixture.Conversion = Context.CreateExpr(
		asAST_EXPR_CONVERSION, ScalarType, asAST_VALUE_PRVALUE, Context.GetSourceManager().MakeRange(File, 4, 5));
	Context.AddExprChild(Fixture.Conversion, Literal);
	Context.SetStmtExpr(Fixture.ConversionStmt, Fixture.Conversion);

	const asASTStmtId CleanupStmt = Context.CreateStmt(asAST_STMT_EXPR, Fixture.Function, asCSourceRange());
	Context.AddStmtChild(Fixture.Loop, CleanupStmt);
	const asASTExprId CleanupValue = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, ScalarType, asAST_VALUE_PRVALUE, asCSourceRange());
	Fixture.Cleanup = Context.CreateExpr(asAST_EXPR_CLEANUP, ScalarType, asAST_VALUE_PRVALUE, asCSourceRange());
	Context.AddExprChild(Fixture.Cleanup, CleanupValue);
	Context.SetResolvedDecl(Fixture.Cleanup, Fixture.Dtor);
	Context.SetStmtExpr(CleanupStmt, Fixture.Cleanup);

	const asASTStmtId CallStmt = Context.CreateStmt(asAST_STMT_EXPR, Fixture.Function, asCSourceRange());
	Context.AddStmtChild(Fixture.Loop, CallStmt);
	Fixture.Receiver = Context.CreateExpr(asAST_EXPR_DECL_REF, ScalarType, asAST_VALUE_LVALUE, asCSourceRange());
	Context.SetResolvedDecl(Fixture.Receiver, Fixture.Function);
	Fixture.Call = Context.CreateExpr(asAST_EXPR_CALL, ScalarType, asAST_VALUE_PRVALUE, asCSourceRange());
	Context.SetResolvedDecl(Fixture.Call, Fixture.Function);
	Context.SetExprReceiver(Fixture.Call, Fixture.Receiver);
	Context.SetStmtExpr(CallStmt, Fixture.Call);

	Context.Seal();
	return Fixture;
}

FString ShadowDiff(const asCASTContext& Left, const asCASTContext& Right)
{
	asCString Diff;
	asCASTShadowDiff(Left, Right, Diff);
	return AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Diff);
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTMatcherAndDiffTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.MatcherAndDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompactTypedMatchersCoverIdentityTypesEdgesCallsControlCleanupAndSource)
	{
		asCASTContext Context;
		const FMatcherFixture Fixture = BuildMatcherFixture(Context, EMatcherFixtureDifference::None);
		ASSERT_THAT(IsTrue(Context.IsPublishable()));

		const AngelscriptNativeTestSupport::FCanonicalASTMatch Decl =
			AngelscriptNativeTestSupport::CanonicalASTUniqueDecl(Context, "F()");
		ASSERT_THAT(IsTrue(Decl.IsMatch(), *Decl.FailureMessage));
		ASSERT_THAT(IsTrue(Decl.Node == asSASTNodeRef::FromDecl(Fixture.Function)));

		const auto ExactType = AngelscriptNativeTestSupport::CanonicalASTExactType(Context, Decl.Node, "int", 0);
		ASSERT_THAT(IsTrue(ExactType.IsMatch(), *ExactType.FailureMessage));
		const auto BodyEdge = AngelscriptNativeTestSupport::CanonicalASTNamedEdge(
			Context, Decl.Node, asAST_EDGE_DECL_BODY, 0);
		ASSERT_THAT(IsTrue(BodyEdge.IsMatch(), *BodyEdge.FailureMessage));
		ASSERT_THAT(IsTrue(BodyEdge.Related == asSASTNodeRef::FromStmt(Fixture.Loop)));

		const auto Callee = AngelscriptNativeTestSupport::CanonicalASTResolvedCallee(Context, "F()");
		ASSERT_THAT(IsTrue(Callee.IsMatch(), *Callee.FailureMessage));
		ASSERT_THAT(IsTrue(Callee.Node == asSASTNodeRef::FromExpr(Fixture.Call)));
		const auto Receiver = AngelscriptNativeTestSupport::CanonicalASTReceiver(
			Context, Fixture.Call, asAST_EXPR_DECL_REF);
		ASSERT_THAT(IsTrue(Receiver.IsMatch(), *Receiver.FailureMessage));
		ASSERT_THAT(IsTrue(Receiver.Related == asSASTNodeRef::FromExpr(Fixture.Receiver)));

		const auto Conversion = AngelscriptNativeTestSupport::CanonicalASTConversion(Context, "int", "int");
		ASSERT_THAT(IsTrue(Conversion.IsMatch(), *Conversion.FailureMessage));
		ASSERT_THAT(IsTrue(Conversion.Node == asSASTNodeRef::FromExpr(Fixture.Conversion)));
		const auto Control = AngelscriptNativeTestSupport::CanonicalASTControlTarget(
			Context, asAST_STMT_CONTINUE, asAST_STMT_WHILE);
		ASSERT_THAT(IsTrue(Control.IsMatch(), *Control.FailureMessage));
		ASSERT_THAT(IsTrue(Control.Node == asSASTNodeRef::FromStmt(Fixture.Continue)));
		ASSERT_THAT(IsTrue(Control.Related == asSASTNodeRef::FromStmt(Fixture.Loop)));
		const auto Cleanup = AngelscriptNativeTestSupport::CanonicalASTCleanup(Context, "T::~T()");
		ASSERT_THAT(IsTrue(Cleanup.IsMatch(), *Cleanup.FailureMessage));
		ASSERT_THAT(IsTrue(Cleanup.Node == asSASTNodeRef::FromExpr(Fixture.Cleanup)));

		const auto Source = AngelscriptNativeTestSupport::CanonicalASTSourceRange(
			Context, Decl.Node, "Matcher/Fixture.as", 0, sizeof("int F() { while (true) { continue; } }") - 1);
		ASSERT_THAT(IsTrue(Source.IsMatch(), *Source.FailureMessage));

		const auto Missing = AngelscriptNativeTestSupport::CanonicalASTUniqueDecl(Context, "Missing()");
		ASSERT_THAT(IsTrue(!Missing.IsMatch() && Missing.FailureMessage.Contains(TEXT("expected exactly one")),
			TEXT("negative matcher should explain cardinality without a dump substring")));
	}

	TEST_METHOD(ShadowDiffMatchesIdenticalGraphsAndReportsFirstTypedNodeFactPath)
	{
		asCASTContext Left;
		asCASTContext Same;
		BuildMatcherFixture(Left, EMatcherFixtureDifference::None);
		BuildMatcherFixture(Same, EMatcherFixtureDifference::None);
		ASSERT_THAT(AreEqual(FString(TEXT("match")), ShadowDiff(Left, Same)));

		asCASTContext TypeDifferent;
		BuildMatcherFixture(TypeDifferent, EMatcherFixtureDifference::Type);
		FString Diff = ShadowDiff(Left, TypeDifferent);
		ASSERT_THAT(IsTrue(Diff.StartsWith(TEXT("mismatch path=T1.primitiveToken")), *Diff));

		asCASTContext DeclDifferent;
		BuildMatcherFixture(DeclDifferent, EMatcherFixtureDifference::Decl);
		Diff = ShadowDiff(Left, DeclDifferent);
		ASSERT_THAT(IsTrue(Diff.Contains(TEXT("path=D1/decl-child[1]/D3.traits")), *Diff));

		asCASTContext StmtDifferent;
		BuildMatcherFixture(StmtDifferent, EMatcherFixtureDifference::Stmt);
		Diff = ShadowDiff(Left, StmtDifferent);
		ASSERT_THAT(IsTrue(Diff.Contains(TEXT("S3.safePointRole")), *Diff));

		asCASTContext ExprDifferent;
		BuildMatcherFixture(ExprDifferent, EMatcherFixtureDifference::Expr);
		Diff = ShadowDiff(Left, ExprDifferent);
		ASSERT_THAT(IsTrue(Diff.Contains(TEXT("E1.literal")), *Diff));
		ASSERT_THAT(IsTrue(!Diff.Contains(TEXT("0x")) && !Diff.Contains(TEXT("EngineLocal"))));
	}
};

#endif
