#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "frontend/AST/as_ast_verifier.h"
#include "as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
void BuildDanglingStmtChildFixture(asCASTContext& Context)
{
	const char Source[] = "void F() {}";
	const asASTFileID File = Context.AddSourceSection(
		"Diagnostics/Diag.as", asAST_SOURCE_AUTHORED, Source, sizeof(Source) - 1, 4);
	const asASTDeclId Tu = Context.CreateTranslationUnit("VerifierDiagnostics");
	const asASTDeclId Fn = Context.CreateDecl(
		asAST_DECL_FUNCTION, Tu, Context.GetSourceManager().MakeRange(File, 0, sizeof(Source) - 1), "F");
	const asASTStmtId Body = Context.CreateStmt(
		asAST_STMT_BLOCK, Fn, Context.GetSourceManager().MakeRange(File, 9, 11));
	Context.SetBody(Fn, Body);
	Context.AddStmtChild(Body, asASTStmtId(99));
}

FString VerifyText(const asSAstVerifyResult& Result)
{
	asCString Text;
	asCASTFormatVerifyResult(Result, Text);
	return AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
}

bool ContainsText(const asCString& Text, const char* Needle)
{
	return FCStringAnsi::Strstr(Text.AddressOf(), Needle) != nullptr;
}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTVerifierDiagnosticTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.VerifierDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DanglingEdgeCarriesNodeTargetSourcePathAndBoundedSubtreeDeterministically)
	{
		asCASTContext First;
		asCASTContext Second;
		BuildDanglingStmtChildFixture(First);
		BuildDanglingStmtChildFixture(Second);
		asSAstVerifyResult FirstResult;
		asSAstVerifyResult SecondResult;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(First, FirstResult)));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Second, SecondResult)));

		ASSERT_THAT(IsTrue(FirstResult.detail.Equals("stmt-child"), TEXT("stable detail token must remain stmt-child")));
		ASSERT_THAT(AreEqual((int)asAST_NODE_STMT, (int)FirstResult.offendingNode.nodeClass));
		ASSERT_THAT(AreEqual(1u, FirstResult.offendingNode.value));
		ASSERT_THAT(IsTrue(FirstResult.offendingKind.Equals("Block"), TEXT("offending canonical kind should be explicit")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD, (int)FirstResult.edgeRole));
		ASSERT_THAT(AreEqual((int)asAST_NODE_STMT, (int)FirstResult.relatedNode.nodeClass));
		ASSERT_THAT(AreEqual(99u, FirstResult.relatedNode.value));
		ASSERT_THAT(IsTrue(FirstResult.relatedKind.Equals("Invalid"), TEXT("dangling target kind should fail closed")));
		ASSERT_THAT(IsTrue(FirstResult.logicalSource.Equals("Diagnostics/Diag.as")));
		ASSERT_THAT(AreEqual(5, FirstResult.line));
		ASSERT_THAT(AreEqual(10, FirstResult.column));
		ASSERT_THAT(IsTrue(FirstResult.rootPath.Equals(SecondResult.rootPath), TEXT("root path must be deterministic")));
		ASSERT_THAT(IsTrue(FirstResult.localSubtree.Equals(SecondResult.localSubtree), TEXT("local subtree must be deterministic")));
		ASSERT_THAT(IsTrue(ContainsText(FirstResult.rootPath, "D1") && ContainsText(FirstResult.rootPath, "D2") && ContainsText(FirstResult.rootPath, "S1"),
			TEXT("path should lead from translation unit through function to the offending statement")));
		ASSERT_THAT(IsTrue(ContainsText(FirstResult.localSubtree, "root=S1") && ContainsText(FirstResult.localSubtree, "target=S99"),
			TEXT("bounded subtree should retain the malformed named edge")));

		const FString Text = VerifyText(FirstResult);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("node=S1")) && Text.Contains(TEXT("kind=Block"))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("edge=stmt-child")) && Text.Contains(TEXT("related=S99"))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("source=\"Diagnostics/Diag.as\" line=5 column=10"))));
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("0x")) && !Text.Contains(TEXT("EngineLocal")),
			TEXT("enriched diagnostics must remain pointer and Engine-ID free")));
	}

	TEST_METHOD(ForeignWrongKindOwnerControlCycleAndCleanupKeepStableCategoriesAndDetails)
	{
		asSAstVerifyResult Result;

		asCASTContext Foreign;
		Foreign.CreateTranslationUnit("Foreign");
		const asCQualType ForeignInt = Foreign.InternPrimitive(ttInt, 0);
		const asASTExprId ForeignExpr = Foreign.CreateExpr(asAST_EXPR_INTEGER_LITERAL, ForeignInt, asAST_VALUE_PRVALUE, asCSourceRange());
		Foreign.SetExprId(ForeignExpr, asASTExprId(99));
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Foreign, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-id") && Result.offendingNode == asSASTNodeRef::FromExpr(ForeignExpr)));

		asCASTContext WrongKind;
		const asASTDeclId WrongTu = WrongKind.CreateTranslationUnit("WrongKind");
		const asASTDeclId BrokenDecl = WrongKind.CreateDecl(asAST_DECL_FUNCTION, WrongTu, asCSourceRange(), "Broken");
		WrongKind.SetDeclKind(BrokenDecl, asAST_DECL_INVALID);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(WrongKind, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("decl-kind") && Result.offendingNode == asSASTNodeRef::FromDecl(BrokenDecl)));

		asCASTContext Owner;
		Owner.CreateTranslationUnit("Owner");
		const asASTStmtId OwnerStmt = Owner.CreateStmt(asAST_STMT_BLOCK, asASTDeclId(99), asCSourceRange());
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_FOREIGN_ID, asCASTVerify(Owner, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-owner") && Result.offendingNode == asSASTNodeRef::FromStmt(OwnerStmt)));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_OWNER, (int)Result.edgeRole));
		ASSERT_THAT(AreEqual(99u, Result.relatedNode.value));

		asCASTContext Control;
		const asASTDeclId ControlTu = Control.CreateTranslationUnit("Control");
		const asASTStmtId LoopA = Control.CreateStmt(asAST_STMT_WHILE, ControlTu, asCSourceRange());
		const asASTStmtId LoopB = Control.CreateStmt(asAST_STMT_WHILE, ControlTu, asCSourceRange());
		const asASTStmtId Break = Control.CreateStmt(asAST_STMT_BREAK, ControlTu, asCSourceRange());
		Control.AddStmtChild(LoopA, Break);
		Control.SetTarget(Break, LoopB);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Control, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-ancestor") && Result.offendingNode == asSASTNodeRef::FromStmt(Break)));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_TARGET, (int)Result.edgeRole));
		ASSERT_THAT(IsTrue(Result.relatedNode == asSASTNodeRef::FromStmt(LoopB)));

		asCASTContext Cycle;
		const asASTDeclId CycleTu = Cycle.CreateTranslationUnit("Cycle");
		const asASTDeclId CycleFn = Cycle.CreateDecl(asAST_DECL_FUNCTION, CycleTu, asCSourceRange(), "F");
		const asASTStmtId BlockA = Cycle.CreateStmt(asAST_STMT_BLOCK, CycleFn, asCSourceRange());
		const asASTStmtId BlockB = Cycle.CreateStmt(asAST_STMT_BLOCK, CycleFn, asCSourceRange());
		Cycle.SetBody(CycleFn, BlockA);
		Cycle.AddStmtChild(BlockA, BlockB);
		Cycle.AddStmtChild(BlockB, BlockA);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Cycle, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("stmt-cycle")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_CHILD, (int)Result.edgeRole));
		ASSERT_THAT(IsTrue(Result.offendingNode.IsValid() && Result.relatedNode.IsValid()));

		asCASTContext Cleanup;
		const asASTDeclId CleanupTu = Cleanup.CreateTranslationUnit("Cleanup");
		const asCQualType CleanupInt = Cleanup.InternPrimitive(ttInt, 0);
		const asASTDeclId NotDtor = Cleanup.CreateDecl(asAST_DECL_FUNCTION, CleanupTu, asCSourceRange(), "NotDtor");
		const asASTExprId Inner = Cleanup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, CleanupInt, asAST_VALUE_PRVALUE, asCSourceRange());
		const asASTExprId CleanupExpr = Cleanup.CreateExpr(asAST_EXPR_CLEANUP, CleanupInt, asAST_VALUE_PRVALUE, asCSourceRange());
		Cleanup.AddExprChild(CleanupExpr, Inner);
		Cleanup.SetResolvedDecl(CleanupExpr, NotDtor);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Cleanup, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("cleanup-dtor") && Result.offendingNode == asSASTNodeRef::FromExpr(CleanupExpr)));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_EXPR_RESOLVED_DECL, (int)Result.edgeRole));
		ASSERT_THAT(IsTrue(Result.relatedNode == asSASTNodeRef::FromDecl(NotDtor)));
	}

	TEST_METHOD(FailedCallPublicationIsMutationFreeAndPointsAtCall)
	{
		const char Source[] = "int F() { return Missing(); }";
		asCASTContext Context;
		const asASTFileID File = Context.AddSourceSection("Diagnostics/Call.as", asAST_SOURCE_AUTHORED, Source, sizeof(Source) - 1, 0);
		const asASTDeclId Tu = Context.CreateTranslationUnit("CallPublication");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, Context.GetSourceManager().MakeRange(File, 0, sizeof(Source) - 1), "F");
		Context.SetDeclType(Fn, IntType);
		const asASTStmtId Return = Context.CreateStmt(asAST_STMT_RETURN, Fn, Context.GetSourceManager().MakeRange(File, 10, 27));
		Context.SetBody(Fn, Return);
		const asASTExprId Call = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, Context.GetSourceManager().MakeRange(File, 17, 26));
		Context.SetStmtExpr(Return, Call);
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("structural verification intentionally permits unresolved call until publication")));

		asCString Before;
		asCASTDump(Context, Before);
		const asUINT DeclCount = Context.GetDeclCount();
		const asUINT StmtCount = Context.GetStmtCount();
		const asUINT ExprCount = Context.GetExprCount();
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerifyPublication(Context, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("call-decl"), TEXT("stable publication detail must remain call-decl")));
		ASSERT_THAT(IsTrue(Result.offendingNode == asSASTNodeRef::FromExpr(Call)));
		ASSERT_THAT(IsTrue(Result.offendingKind.Equals("Call")));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_EXPR_RESOLVED_DECL, (int)Result.edgeRole));
		ASSERT_THAT(IsTrue(!Result.relatedNode.IsValid(), TEXT("missing callee has no invented target")));
		ASSERT_THAT(IsTrue(ContainsText(Result.rootPath, "D1") && ContainsText(Result.rootPath, "E1")));
		ASSERT_THAT(IsTrue(ContainsText(Result.localSubtree, "root=E1")));

		asCString After;
		asCASTDump(Context, After);
		ASSERT_THAT(IsTrue(Context.IsPublishable(), TEXT("failed publication must not unfreeze or mutate the snapshot")));
		ASSERT_THAT(AreEqual(DeclCount, Context.GetDeclCount()));
		ASSERT_THAT(AreEqual(StmtCount, Context.GetStmtCount()));
		ASSERT_THAT(AreEqual(ExprCount, Context.GetExprCount()));
		ASSERT_THAT(IsTrue(Before == After, TEXT("failed publication must be byte-for-byte mutation free")));
	}

	TEST_METHOD(LocalSubtreeHasHardNodeAndDepthBounds)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("Bounded");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Call = Context.CreateExpr(asAST_EXPR_CALL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		for (int Index = 0; Index < 20; ++Index)
		{
			const asASTExprId Arg = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
			Context.AddExprChild(Call, Arg);
		}
		Context.SetResolvedDecl(Call, asASTDeclId(99));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_DANGLING_ID, asCASTVerify(Context, Result)));
		ASSERT_THAT(IsTrue(Result.detail.Equals("expr-decl")));
		ASSERT_THAT(IsTrue(ContainsText(Result.localSubtree, "truncated=true"),
			TEXT("large malformed subtrees should say that the hard diagnostic bound truncated them")));
		ASSERT_THAT(IsTrue(Result.localSubtree.GetLength() < 2048,
			TEXT("diagnostic local subtree must stay bounded even for a wide graph")));
	}
};

#endif
