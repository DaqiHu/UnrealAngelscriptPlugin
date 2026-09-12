#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/AST/as_ast_context.h"
#include "source/as_ast_dump.h"
#include "frontend/AST/as_ast_verifier.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTDumpTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.Dump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RepeatedBuildsDumpIdenticallyWithoutPointers)
	{
		auto BuildSample = [](asCASTContext& Context) -> int
		{
			const asASTDeclId Tu = Context.CreateTranslationUnit("DumpMod");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
			Context.SetDeclType(Fn, IntType);
			const asASTStmtId Body = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
			Context.SetBody(Fn, Body);
			const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Lit, "7");
			Context.SetStmtExpr(Body, Lit);
			return Context.Seal();
		};

		asCASTContext First;
		asCASTContext Second;
		ASSERT_THAT(AreEqual(0, BuildSample(First), TEXT("first sample graph should verify and seal")));
		ASSERT_THAT(AreEqual(0, BuildSample(Second), TEXT("second sample graph should verify and seal")));
		ASSERT_THAT(IsTrue(AngelscriptNativeTestSupport::CanonicalAstDumpsEqual(First, Second),
			TEXT("identical graphs must dump byte-identically")));

		asCString Dump;
		asCASTDump(First, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(!Text.Contains(TEXT("0x")), TEXT("dump must not spell pointer addresses")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("DECL id=2 kind=Function name=F")), TEXT("dump should name declarations by stable id/kind")));
	}

	TEST_METHOD(FormatVerifyResultOkIsAddressFreeAndStable)
	{
		asSAstVerifyResult Result;
		asCString Text;
		Text = "junk";
		asCASTFormatVerifyResult(Result, Text);
		const FString Line = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
		ASSERT_THAT(IsTrue(Line.Equals(TEXT("VERIFY category=OK detail= range=0:0-0:0\n")),
			TEXT("OK default result must be one address-free VERIFY line with empty detail and zero range")));
		ASSERT_THAT(IsTrue(!Line.Contains(TEXT("0x")), TEXT("formatter must not spell pointers")));
		ASSERT_THAT(IsTrue(!Line.Contains(TEXT("junk")), TEXT("formatter must replace out, not append")));

		Result.range.begin = asCSourceLocation(1, 0);
		Result.range.end = asCSourceLocation(2, 4);
		asCASTFormatVerifyResult(Result, Text);
		const FString Invalid = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
		ASSERT_THAT(IsTrue(Invalid.Equals(TEXT("VERIFY category=OK detail= range=0:0-0:0\n")),
			TEXT("invalid range must print range=0:0-0:0, not the raw fields")));
	}

	TEST_METHOD(FormatVerifyResultUnsealedPublication)
	{
		asSAstVerifyResult Result;
		Result.category = asAST_VERIFY_UNSEALED_PUBLICATION;
		Result.detail = "unsealed-publication";
		asCString Text;
		asCASTFormatVerifyResult(Result, Text);
		const FString Line = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
		ASSERT_THAT(IsTrue(Line.Equals(TEXT("VERIFY category=UNSEALED_PUBLICATION detail=unsealed-publication range=0:0-0:0\n")),
			TEXT("UNSEALED_PUBLICATION token must drop the asAST_VERIFY_ prefix")));
		ASSERT_THAT(IsTrue(!Line.Contains(TEXT("0x")), TEXT("formatter must not spell pointers")));
	}

	TEST_METHOD(FormatVerifyResultDanglingIdWithDetailAndRange)
	{
		asSAstVerifyResult Result;
		Result.category = asAST_VERIFY_DANGLING_ID;
		Result.detail = "expr-id";
		Result.range.begin = asCSourceLocation(1, 4);
		Result.range.end = asCSourceLocation(1, 8);
		asCString Text;
		asCASTFormatVerifyResult(Result, Text);
		const FString Line = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
		ASSERT_THAT(IsTrue(Line.Equals(TEXT("VERIFY category=DANGLING_ID detail=expr-id range=1:4-1:8\n")),
			TEXT("DANGLING_ID must print detail token and valid fileID:offset range")));
		ASSERT_THAT(IsTrue(!Line.Contains(TEXT("0x")), TEXT("formatter must not spell pointers")));
	}

	TEST_METHOD(FormatVerifyResultCategoryTokensCoverEveryEnumerator)
	{
		struct FCategoryToken
		{
			asEASTVerifyCategory Category;
			const TCHAR* Expected;
		};
		const FCategoryToken Rows[] = {
			{ asAST_VERIFY_OK, TEXT("VERIFY category=OK detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_DANGLING_ID, TEXT("VERIFY category=DANGLING_ID detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_FOREIGN_ID, TEXT("VERIFY category=FOREIGN_ID detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_WRONG_KIND, TEXT("VERIFY category=WRONG_KIND detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_INVALID_TYPE, TEXT("VERIFY category=INVALID_TYPE detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_INVALID_RANGE, TEXT("VERIFY category=INVALID_RANGE detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_INVALID_CHILD, TEXT("VERIFY category=INVALID_CHILD detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_POST_SEAL_MUTATION, TEXT("VERIFY category=POST_SEAL_MUTATION detail= range=0:0-0:0\n") },
			{ asAST_VERIFY_UNSEALED_PUBLICATION, TEXT("VERIFY category=UNSEALED_PUBLICATION detail= range=0:0-0:0\n") },
		};
		for (const FCategoryToken& Row : Rows)
		{
			asSAstVerifyResult Result;
			Result.category = Row.Category;
			asCString Text;
			asCASTFormatVerifyResult(Result, Text);
			const FString Line = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Text);
			ASSERT_THAT(IsTrue(Line.Equals(Row.Expected),
				TEXT("VerifyCategoryName must cover every asEASTVerifyCategory enumerator")));
			ASSERT_THAT(IsTrue(!Line.Contains(TEXT("0x")), TEXT("category tokens must stay address-free")));
		}

		asSAstVerifyResult Unknown;
		Unknown.category = static_cast<asEASTVerifyCategory>(255);
		asCString UnknownText;
		asCASTFormatVerifyResult(Unknown, UnknownText);
		const FString UnknownLine = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(UnknownText);
		ASSERT_THAT(IsTrue(UnknownLine.Equals(TEXT("VERIFY category=UNKNOWN detail= range=0:0-0:0\n")),
			TEXT("unknown asEASTVerifyCategory byte must print UNKNOWN, not a pointer or integer dump")));
	}

	TEST_METHOD(DumpOnVerifyFailIncludesAstHeaderOnlyOnFailure)
	{
		auto BuildSample = [](asCASTContext& Context) -> int
		{
			const asASTDeclId Tu = Context.CreateTranslationUnit("DumpMod");
			const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
			const asASTDeclId Fn = Context.CreateDecl(asAST_DECL_FUNCTION, Tu, asCSourceRange(), "F");
			Context.SetDeclType(Fn, IntType);
			const asASTStmtId Body = Context.CreateStmt(asAST_STMT_RETURN, Fn, asCSourceRange());
			Context.SetBody(Fn, Body);
			const asASTExprId Lit = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
			Context.SetLiteral(Lit, "7");
			Context.SetStmtExpr(Body, Lit);
			return Context.Seal();
		};

		asCASTContext Sealed;
		ASSERT_THAT(AreEqual(0, BuildSample(Sealed), TEXT("sealed DumpMod sample must verify and seal")));

		asSAstVerifyResult Ok;
		asCString OkText;
		OkText = "junk";
		asCASTDumpOnVerifyFail(Sealed, Ok, OkText);
		const FString OkLine = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(OkText);
		ASSERT_THAT(IsTrue(OkLine.Equals(TEXT("VERIFY category=OK detail= range=0:0-0:0\n")),
			TEXT("OK dump-on-fail must be the formatter line only")));
		ASSERT_THAT(IsTrue(!OkLine.Contains(TEXT("AST\n")), TEXT("OK path must not append the AST dump header")));
		ASSERT_THAT(IsTrue(!OkLine.Contains(TEXT("DECL")), TEXT("OK path must not contain DECL lines")));
		ASSERT_THAT(IsTrue(!OkLine.Contains(TEXT("0x")), TEXT("OK dump-on-fail must not spell pointers")));

		asSAstVerifyResult Fail;
		Fail.category = asAST_VERIFY_DANGLING_ID;
		Fail.detail = "translation-unit";
		asCString FailText;
		asCASTDumpOnVerifyFail(Sealed, Fail, FailText);
		const FString Combined = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(FailText);
		ASSERT_THAT(IsTrue(Combined.StartsWith(TEXT("VERIFY category=DANGLING_ID detail=translation-unit range=0:0-0:0\n")),
			TEXT("fail path must lead with the formatter line")));
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT("\nAST\n")), TEXT("fail path must append asCASTDump, which starts with AST\\n")));
		ASSERT_THAT(IsTrue(Combined.Contains(TEXT("DECL id=2 kind=Function name=F")),
			TEXT("fail path dump must be the context dump, not an empty string")));
		ASSERT_THAT(IsTrue(!Combined.Contains(TEXT("0x")), TEXT("combined dump must stay address-free")));
	}

	TEST_METHOD(MalformedGraphDumpStillInventoriesNodesAfterDanglingEdge)
	{
		asCASTContext Context;
		Context.CreateTranslationUnit("MalformedDump");
		const asCQualType IntType = Context.InternPrimitive(ttInt, 0);
		const asASTExprId Broken = Context.CreateExpr(asAST_EXPR_UNARY, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Context.AddExprChild(Broken, asASTExprId(99));
		const asASTExprId Later = Context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Context.SetLiteral(Later, "42");

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("EXPR id=1 kind=Unary")), TEXT("dump should retain the malformed source node")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("EXPR id=2 kind=IntegerLiteral")),
			TEXT("diagnostic inventory must continue after a dangling edge instead of truncating the dump")));
	}
};

#endif
