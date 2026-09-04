#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "source/as_decl.h"
#include "source/as_expr.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused SemaAuthority slices live here so
// AngelscriptNativeCanonicalASTSemaAuthorityTests.cpp does not keep growing.
// Prefix stays under Compiler.CanonicalAST.SemaAuthority so the AST-first gate
// command still discovers these methods.

namespace CanonicalASTSemaHandleCompareTest
{
	static void HandleAddRef(asIScriptGeneric*)
	{
	}

	static void HandleRelease(asIScriptGeneric*)
	{
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaHandleCompareTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.HandleCompare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(HandleInequalityRemainsBuiltinBinaryNotOpEquals)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CObj", asBEHAVE_ADDREF, "void f()",
				asFUNCTION(CanonicalASTSemaHandleCompareTest::HandleAddRef), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CObj", asBEHAVE_RELEASE, "void f()",
				asFUNCTION(CanonicalASTSemaHandleCompareTest::HandleRelease), asCALL_GENERIC) >= 0,
			TEXT("handle inequality fixture registers a host REF type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL)));

		asIScriptModule* const Module = ScriptEngine->GetModule(
			"SemaHandleInequality", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaHandleInequality module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				CObj Left = nullptr;
				CObj Right = nullptr;
				if (Left != Right)
				{
					return 1;
				}
				return 0;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"SemaHandleInequality.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("handle inequality module must compile under CANONICAL")));

		const asCASTContext* const Context =
			static_cast<asCModule*>(Module)->GetCanonicalASTContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("handle inequality must retain a sealed AST")));
		if (Context == nullptr)
		{
			return;
		}

		asCString Dump;
		asCASTDump(*Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("handle != must stay a builtin Binary, not opEquals. dump:\n%s"),
			*Text);

		const asCExpr* Inequality = nullptr;
		asUINT InequalityCount = 0;
		asUINT OpEqualsCount = 0;
		for (asUINT ExprIndex = 1; ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context->GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr)
			{
				continue;
			}
			if (Expr->kind == asAST_EXPR_BINARY && Expr->literal.Equals("!="))
			{
				Inequality = Expr;
				++InequalityCount;
			}
			const asCDecl* const Callee = Context->GetDecl(Expr->resolvedDecl);
			if (Callee != nullptr && Callee->name.Equals("opEquals"))
			{
				++OpEqualsCount;
			}
		}

		ASSERT_THAT(AreEqual(static_cast<asUINT>(1), InequalityCount, *DumpMsg));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), OpEqualsCount, *DumpMsg));
		ASSERT_THAT(IsNotNull(Inequality, *DumpMsg));
		if (Inequality == nullptr || Inequality->children.GetLength() != 2)
		{
			return;
		}

		const asCExpr* const Left = Context->GetExpr(Inequality->children[0]);
		const asCExpr* const Right = Context->GetExpr(Inequality->children[1]);
		ASSERT_THAT(IsNotNull(Left, *DumpMsg));
		ASSERT_THAT(IsNotNull(Right, *DumpMsg));
		if (Left == nullptr || Right == nullptr)
		{
			return;
		}
		const asCType* const LeftType = Context->GetType(Left->type.type);
		const asCType* const RightType = Context->GetType(Right->type.type);
		ASSERT_THAT(IsTrue(
			LeftType != nullptr
				&& (Left->type.IsHandle()
					|| LeftType->kind == asAST_TYPE_REFERENCE_OBJECT),
			*FString::Printf(
				TEXT("left operand of handle != must be a REF/handle type; kind=%d handle=%s. dump:\n%s"),
				LeftType != nullptr ? static_cast<int>(LeftType->kind) : -1,
				Left->type.IsHandle() ? TEXT("yes") : TEXT("no"),
				*Text)));
		ASSERT_THAT(IsTrue(
			RightType != nullptr
				&& (Right->type.IsHandle()
					|| RightType->kind == asAST_TYPE_REFERENCE_OBJECT),
			*FString::Printf(
				TEXT("right operand of handle != must be a REF/handle type; kind=%d handle=%s. dump:\n%s"),
				RightType != nullptr ? static_cast<int>(RightType->kind) : -1,
				Right->type.IsHandle() ? TEXT("yes") : TEXT("no"),
				*Text)));
		ASSERT_THAT(IsFalse(Inequality->resolvedDecl.IsValid(),
			TEXT("builtin handle != must not resolve to a method declaration")));
	}
};

#endif
