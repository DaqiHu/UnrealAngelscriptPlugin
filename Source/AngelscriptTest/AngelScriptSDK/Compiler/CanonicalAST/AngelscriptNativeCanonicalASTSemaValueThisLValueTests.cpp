#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "source/as_builder.h"
#include "source/as_decl.h"
#include "source/as_expr.h"
#include "source/as_generic.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused SemaAuthority slice: VALUE-struct `this` passed to a generic ?&
// formal. Prefix stays under Compiler.CanonicalAST.SemaAuthority.

namespace CanonicalASTSemaValueThisLValueTest
{
	static void ObserveWildcard(asIScriptGeneric*)
	{
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaValueThisLValueTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.ValueThisLValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValueStructThisToWildcardRefRemainsSealedThisArgument)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"void Observe(?& Value)",
			asFUNCTION(CanonicalASTSemaValueThisLValueTest::ObserveWildcard),
			asCALL_GENERIC) >= 0,
			TEXT("value-this lvalue observer must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaValueThisLValue");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaValueThisLValue module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FHost
			{
				int Value;

				void Touch()
				{
					Observe(this);
				}
			}
			)AS");
		Code.SetCode("SemaValueThisLValue.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("VALUE-struct this to ?& must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("VALUE-struct this to ?& must seal without CodeGen")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Observe(this) must seal a ThisExpr typed as FHost and a Call with that lvalue. dump:\n%s"),
			*Text);

		const asCExpr* ThisExpr = nullptr;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr != nullptr && Expr->kind == asAST_EXPR_THIS)
			{
				ThisExpr = Expr;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ThisExpr, *DumpMsg));
		if (ThisExpr == nullptr)
		{
			return;
		}

		const asCType* const ThisType = Context.GetType(ThisExpr->type.type);
		ASSERT_THAT(IsTrue(
			ThisType != nullptr
				&& ThisType->stableKey.Equals("FHost")
				&& ThisType->kind == asAST_TYPE_VALUE_OBJECT
				&& !ThisExpr->type.IsHandle()
				&& ThisExpr->valueCategory == asAST_VALUE_LVALUE,
			*DumpMsg));

		const asCExpr* Call = nullptr;
		const asCDecl* Callee = nullptr;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
			{
				continue;
			}
			const asCDecl* const Target = Context.GetDecl(Expr->resolvedDecl);
			if (Target != nullptr && Target->name.Equals("Observe"))
			{
				Call = Expr;
				Callee = Target;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Call, *DumpMsg));
		ASSERT_THAT(IsNotNull(Callee, *DumpMsg));
		if (Call == nullptr || Callee == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Call->children.GetLength() >= 1, *DumpMsg));
		const asCExpr* Argument = Context.GetExpr(Call->children[0]);
		while (Argument != nullptr
			&& (Argument->kind == asAST_EXPR_OPAQUE_VALUE
				|| Argument->kind == asAST_EXPR_CLEANUP
				|| Argument->kind == asAST_EXPR_SEQUENCE
				|| Argument->kind == asAST_EXPR_CONVERSION)
			&& Argument->children.GetLength() > 0)
		{
			const asUINT ChildIndex = Argument->kind == asAST_EXPR_SEQUENCE
				? Argument->children.GetLength() - 1
				: 0;
			Argument = Context.GetExpr(Argument->children[ChildIndex]);
		}
		ASSERT_THAT(IsTrue(
			Argument != nullptr && Argument->kind == asAST_EXPR_THIS,
			*DumpMsg));

		const asCDecl* Formal = nullptr;
		for (asUINT ChildIndex = 0; ChildIndex < Callee->children.GetLength(); ++ChildIndex)
		{
			const asCDecl* const Param = Context.GetDecl(Callee->children[ChildIndex]);
			if (Param != nullptr && Param->kind == asAST_DECL_PARAM)
			{
				Formal = Param;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Formal, *DumpMsg));
		if (Formal == nullptr)
		{
			return;
		}
		const asCType* const FormalType = Context.GetType(Formal->type.type);
		ASSERT_THAT(IsTrue(
			FormalType != nullptr
				&& FormalType->kind == asAST_TYPE_WILDCARD
				&& Formal->type.IsReference()
				&& !Formal->type.IsHandle(),
			*DumpMsg));
	}
};

#endif
