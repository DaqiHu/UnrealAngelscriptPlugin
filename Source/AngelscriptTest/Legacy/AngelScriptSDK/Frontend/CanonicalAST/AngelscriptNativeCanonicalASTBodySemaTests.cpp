#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "frontend/as_ast_verifier.h"
#include "frontend/as_parser.h"
#include "source/as_scriptcode.h"
#include "frontend/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <cstring>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalASTBodySemaTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.BodySema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExpressionsCallsSequenceAndControlLowerToCanonicalAST)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(ScriptEngine, "BodySema");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		Code.SetCode("BodySema",
			"int Add(int A, int B) { return A + B; } int F(int X) { int Y = X; if (X > 0) { Y = -X; } while (Y < 4) { Y += 1; } switch (Y) { case 1: break; default: Y = 2; } return Add(1, Y ? 2 : 3); }",
			true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code), TEXT("body Sema source should parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("body Sema graph should verify")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=DeclRef")), TEXT("parameter/local refs")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Binary")), TEXT("arithmetic")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Unary")) || Text.Contains(TEXT("kind=Assign")), TEXT("unary/assign")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Call")) && Text.Contains(TEXT("reverse-formal")), TEXT("call reverse formal order")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Conditional")), TEXT("ternary")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=If")), TEXT("if")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=While")), TEXT("while")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Switch")) && Text.Contains(TEXT("kind=Case")), TEXT("switch/case")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Break")), TEXT("break")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Sequence")) || Text.Contains(TEXT("kind=Assign")), TEXT("assignment sequencing")));
	}

	TEST_METHOD(LexicalShadowingKeepsDistinctDeclsAndRestoresOuterBinding)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BodySemaLexicalShadow");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		static constexpr const char* Source =
			"int F() { int X = 1; { int X = 2; X += 10; } return X; }";
		asCScriptCode Code;
		Code.SetCode("BodySemaLexicalShadow", Source, true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("lexical-shadow source should parse")));
		Sema.ResolveLexicalScopes();

		const asCDecl* Function = nullptr;
		asCArray<const asCDecl*> ShadowDecls;
		FString ShadowDeclInventory;
		for (asUINT DeclIndex = 1; DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(DeclIndex));
			if (Decl != nullptr && Decl->name.Equals("X"))
			{
				ShadowDeclInventory += FString::Printf(
					TEXT("id=%u kind=%d parent=%u range=%u:%u-%u; "),
					Decl->id.value, static_cast<int32>(Decl->kind), Decl->parent.value,
					Decl->range.begin.fileID, Decl->range.begin.offset,
					Decl->range.end.offset);
			}
			if (Decl != nullptr && Decl->kind == asAST_DECL_FUNCTION
				&& Decl->name.Equals("F"))
			{
				Function = Decl;
			}
		}
		if (Function != nullptr)
		{
			for (asUINT ChildIndex = 0;
				ChildIndex < Function->children.GetLength(); ++ChildIndex)
			{
				const asCDecl* const Child = Context.GetDecl(
					Function->children[ChildIndex]);
				if (Child != nullptr && Child->kind == asAST_DECL_VAR
					&& Child->name.Equals("X"))
				{
					ShadowDecls.PushLast(Child);
				}
			}
		}

		ASSERT_THAT(IsNotNull(Function,
			TEXT("lexical-shadow AST should contain the exact function")));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(2), ShadowDecls.GetLength(),
			*FString::Printf(
				TEXT("outer and inner X must be distinct VarDecl identities. Inventory={%s}"),
				*ShadowDeclInventory)));
		if (ShadowDecls.GetLength() != 2)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ShadowDecls[0]->id != ShadowDecls[1]->id,
			TEXT("nested shadow declarations must never collapse by spelling")));

		const asUINT InnerUseOffset = static_cast<asUINT>(
			std::strstr(Source, "X += 10") - Source);
		const asUINT ReturnUseOffset = static_cast<asUINT>(
			std::strstr(Source, "return X") - Source + std::strlen("return "));
		const asCExpr* InnerUse = nullptr;
		const asCExpr* ReturnUse = nullptr;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_DECL_REF
				|| !Expr->literal.Equals("X"))
			{
				continue;
			}
			if (Expr->range.begin.offset == InnerUseOffset)
			{
				InnerUse = Expr;
			}
			else if (Expr->range.begin.offset == ReturnUseOffset)
			{
				ReturnUse = Expr;
			}
		}
		ASSERT_THAT(IsNotNull(InnerUse,
			TEXT("lexical-shadow AST should expose the inner compound-assignment reference")));
		ASSERT_THAT(IsNotNull(ReturnUse,
			TEXT("lexical-shadow AST should expose the post-block return reference")));
		if (InnerUse != nullptr && ReturnUse != nullptr)
		{
			ASSERT_THAT(IsTrue(InnerUse->resolvedDecl == ShadowDecls[1]->id,
				TEXT("inner use must bind the innermost active VarDecl")));
			ASSERT_THAT(IsTrue(ReturnUse->resolvedDecl == ShadowDecls[0]->id,
				TEXT("leaving the block must restore the outer VarDecl binding")));
		}
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("lexical-shadow graph should verify after exact binding")));
	}

	TEST_METHOD(ForInitLexicalScopeBindsConditionBodyIncrementAndRestoresOuter)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BodySemaForLexicalScope");
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		static constexpr const char* Source =
			"int F() { int I = 100; int Sum = 0; for (int I = 0; I < 3; ++I) { Sum += I; } return Sum + I; }";
		asCScriptCode Code;
		Code.SetCode("BodySemaForLexicalScope", Source, true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("for lexical-scope source should parse")));
		Sema.ResolveLexicalScopes();

		const asCDecl* OuterI = nullptr;
		const asCDecl* LoopI = nullptr;
		FString DeclInventory;
		for (asUINT DeclIndex = 1; DeclIndex <= Context.GetDeclCount(); ++DeclIndex)
		{
			const asCDecl* const Decl = Context.GetDecl(asASTDeclId(DeclIndex));
			if (Decl == nullptr || Decl->kind != asAST_DECL_VAR
				|| !Decl->name.Equals("I"))
			{
				continue;
			}
			DeclInventory += FString::Printf(
				TEXT("id=%u parent=%u range=%u:%u-%u; "),
				Decl->id.value, Decl->parent.value, Decl->range.begin.fileID,
				Decl->range.begin.offset, Decl->range.end.offset);
			if (OuterI == nullptr || Decl->range.begin.offset < OuterI->range.begin.offset)
			{
				LoopI = OuterI;
				OuterI = Decl;
			}
			else if (LoopI == nullptr || Decl->range.begin.offset < LoopI->range.begin.offset)
			{
				LoopI = Decl;
			}
		}

		ASSERT_THAT(IsNotNull(OuterI,
			TEXT("for lexical-scope AST should retain the outer I declaration")));
		ASSERT_THAT(IsNotNull(LoopI,
			*FString::Printf(
				TEXT("for lexical-scope AST should retain a distinct loop I. Inventory={%s}"),
				*DeclInventory)));
		if (OuterI == nullptr || LoopI == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OuterI->id != LoopI->id,
			TEXT("for initializer shadowing must create a distinct VarDecl")));

		const asUINT ConditionOffset = static_cast<asUINT>(
			std::strstr(Source, "I < 3") - Source);
		const asUINT IncrementOffset = static_cast<asUINT>(
			std::strstr(Source, "++I") - Source + std::strlen("++"));
		const asUINT BodyOffset = static_cast<asUINT>(
			std::strstr(Source, "Sum += I") - Source + std::strlen("Sum += "));
		const asUINT ReturnOffset = static_cast<asUINT>(
			std::strstr(Source, "return Sum + I") - Source
			+ std::strlen("return Sum + "));
		const asCExpr* ConditionRef = nullptr;
		const asCExpr* IncrementRef = nullptr;
		const asCExpr* BodyRef = nullptr;
		const asCExpr* ReturnRef = nullptr;
		FString RefInventory;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_DECL_REF
				|| !Expr->literal.Equals("I"))
			{
				continue;
			}
			RefInventory += FString::Printf(
				TEXT("expr=%u offset=%u resolved=%u; "),
				Expr->id.value, Expr->range.begin.offset, Expr->resolvedDecl.value);
			if (Expr->range.begin.offset == ConditionOffset)
			{
				ConditionRef = Expr;
			}
			else if (Expr->range.begin.offset == IncrementOffset)
			{
				IncrementRef = Expr;
			}
			else if (Expr->range.begin.offset == BodyOffset)
			{
				BodyRef = Expr;
			}
			else if (Expr->range.begin.offset == ReturnOffset)
			{
				ReturnRef = Expr;
			}
		}

		ASSERT_THAT(IsNotNull(ConditionRef,
			TEXT("for condition should expose its loop-variable DeclRef")));
		ASSERT_THAT(IsNotNull(IncrementRef,
			TEXT("for increment should expose its loop-variable DeclRef")));
		ASSERT_THAT(IsNotNull(BodyRef,
			TEXT("for body should expose its loop-variable DeclRef")));
		ASSERT_THAT(IsNotNull(ReturnRef,
			TEXT("post-loop return should expose its outer-variable DeclRef")));
		if (ConditionRef != nullptr && IncrementRef != nullptr
			&& BodyRef != nullptr && ReturnRef != nullptr)
		{
			ASSERT_THAT(IsTrue(ConditionRef->resolvedDecl == LoopI->id,
				*FString::Printf(
					TEXT("for condition must bind loop I. Refs={%s}"), *RefInventory)));
			ASSERT_THAT(IsTrue(IncrementRef->resolvedDecl == LoopI->id,
				*FString::Printf(
					TEXT("for increment must bind loop I. Refs={%s}"), *RefInventory)));
			ASSERT_THAT(IsTrue(BodyRef->resolvedDecl == LoopI->id,
				*FString::Printf(
					TEXT("for body must bind loop I. Refs={%s}"), *RefInventory)));
			ASSERT_THAT(IsTrue(ReturnRef->resolvedDecl == OuterI->id,
				*FString::Printf(
					TEXT("leaving for must restore outer I. Refs={%s}"), *RefInventory)));
		}
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("for lexical-scope graph should verify after exact binding")));
	}

	TEST_METHOD(LifetimeCleanupAndTryCatchRejection)
	{
		asCASTContext Context;
		asCSema Sema(nullptr, Context);
		const asASTDeclId Tu = Sema.ActOnTranslationUnit("Life");
		const asCQualType Obj = Context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "FValue", 0);
		asCArray<asASTExprId> Args;
		const asASTExprId Constructed = Sema.ActOnConstruct(Obj, Args, asCSourceRange());
		const asASTExprId Clean = Sema.ActOnCleanup(Constructed, asCSourceRange());
		Sema.ActOnFunctionDecl(Tu, "Make", Obj, asCSourceRange());
		ASSERT_THAT(IsTrue(Context.GetExpr(Constructed) != nullptr, TEXT("construct exists")));
		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=MaterializeTemporary")), TEXT("value construction materializes")));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("kind=Cleanup")), TEXT("cleanup plan is explicit")));
		(void)Clean;
		Sema.AddDiagnostic("try-catch-rejected");
		bool bRejected = false;
		for (asUINT i = 0; i < Sema.GetDiagnostics().GetLength(); ++i)
		{
			if (Sema.GetDiagnostics()[i].Equals("try-catch-rejected"))
			{
				bRejected = true;
			}
		}
		ASSERT_THAT(IsTrue(bRejected, TEXT("try/catch stays rejected")));
		ASSERT_THAT(AreEqual(0, Context.Seal(), TEXT("lifetime graph should verify")));
	}

	TEST_METHOD(VerifierRejectsWrongBreakContinueAndDuplicateCase)
	{
		asCASTContext Context;
		const asASTDeclId Tu = Context.CreateTranslationUnit("Ctrl");
		const asASTStmtId Ret = Context.CreateStmt(asAST_STMT_RETURN, Tu, asCSourceRange());
		const asASTStmtId Brk = Context.CreateStmt(asAST_STMT_BREAK, Tu, asCSourceRange());
		ASSERT_THAT(IsTrue(Brk.IsValid(), TEXT("break stmt")));
		ASSERT_THAT(AreEqual(0, Context.SetTarget(Brk, Ret), TEXT("break targets return")));
		asSAstVerifyResult Result;
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_WRONG_KIND, asCASTVerify(Context, Result), TEXT("break to return is wrong-kind")));
		ASSERT_THAT(IsTrue(Result.detail.Equals("break-target"),
			TEXT("wrong-kind Break keeps the stable control detail")));
		ASSERT_THAT(IsTrue(
			Result.offendingNode == asSASTNodeRef::FromStmt(Brk)));
		ASSERT_THAT(AreEqual((int)asAST_EDGE_STMT_TARGET,
			(int)Result.edgeRole));
		ASSERT_THAT(IsTrue(
			Result.relatedNode == asSASTNodeRef::FromStmt(Ret)));

		asCASTContext Dup;
		const asASTDeclId DupTu = Dup.CreateTranslationUnit("Dup");
		const asCQualType IntType = Dup.InternPrimitive(ttInt, 0);
		const asASTStmtId Sw = Dup.CreateStmt(asAST_STMT_SWITCH, DupTu, asCSourceRange());
		const asASTExprId LitA = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Dup.SetLiteral(LitA, "1");
		Dup.SetLiteralBits(LitA, 1);
		const asASTExprId LitB = Dup.CreateExpr(asAST_EXPR_INTEGER_LITERAL, IntType, asAST_VALUE_PRVALUE, asCSourceRange());
		Dup.SetLiteral(LitB, "1");
		Dup.SetLiteralBits(LitB, 1);
		const asASTStmtId CaseA = Dup.CreateStmt(asAST_STMT_CASE, DupTu, asCSourceRange());
		Dup.SetStmtExpr(CaseA, LitA);
		const asASTStmtId CaseB = Dup.CreateStmt(asAST_STMT_CASE, DupTu, asCSourceRange());
		Dup.SetStmtExpr(CaseB, LitB);
		Dup.AddStmtChild(Sw, CaseA);
		Dup.AddStmtChild(Sw, CaseB);
		ASSERT_THAT(AreEqual((int)asAST_VERIFY_INVALID_CHILD, asCASTVerify(Dup, Result), TEXT("duplicate case fails closed")));
	}
};

#endif
