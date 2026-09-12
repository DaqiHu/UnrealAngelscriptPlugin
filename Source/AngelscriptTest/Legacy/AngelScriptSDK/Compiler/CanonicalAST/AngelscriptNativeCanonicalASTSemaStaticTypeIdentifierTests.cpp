#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "as_builder.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"
#include "as_generic.h"
#include "as_module.h"
#include "frontend/Parser/as_parser.h"
#include "source/as_scriptcode.h"
#include "as_scriptengine.h"
#include "frontend/Sema/as_sema.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused SemaAuthority slice: a type identifier used as a value must resolve
// through the host `__StaticType_<Name>` global, matching
// `__CreateLiteralAsset(UObject, "Name")`. Prefix stays under
// Compiler.CanonicalAST.SemaAuthority.

namespace CanonicalASTSemaStaticTypeIdentifierTest
{
	static void ObserveClass(asIScriptGeneric*)
	{
	}

	static void* StaticTypeValue = reinterpret_cast<void*>(static_cast<uintptr_t>(0x51));
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaStaticTypeIdentifierTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.StaticTypeIdentifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TypeIdentifierArgumentResolvesThroughStaticTypeGlobal)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CClass",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("CClass type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"CObj",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("CObj type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalProperty(
				"CClass __StaticType_CObj",
				&CanonicalASTSemaStaticTypeIdentifierTest::StaticTypeValue) >= 0,
			TEXT("__StaticType_CObj host global must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Observe(CClass Cls)",
				asFUNCTION(CanonicalASTSemaStaticTypeIdentifierTest::ObserveClass),
				asCALL_GENERIC) >= 0,
			TEXT("Observe(CClass) must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaStaticTypeIdentifier");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaStaticTypeIdentifier module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				Observe(CObj);
			}
			)AS");
		Code.SetCode("SemaStaticTypeIdentifier.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("type identifier as value must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("type identifier as value must seal without CodeGen")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Observe(CObj) must seal a Call whose argument is DeclRef __StaticType_CObj. dump:\n%s"),
			*Text);

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
			Argument != nullptr && Argument->kind == asAST_EXPR_DECL_REF,
			*DumpMsg));
		if (Argument == nullptr)
		{
			return;
		}
		const asCDecl* const Target = Context.GetDecl(Argument->resolvedDecl);
		ASSERT_THAT(IsTrue(
			Target != nullptr
				&& Target->kind == asAST_DECL_VAR
				&& Target->name.Equals("__StaticType_CObj"),
			*DumpMsg));
		const asCType* const ArgumentType = Context.GetType(Argument->type.type);
		ASSERT_THAT(IsTrue(
			ArgumentType != nullptr && ArgumentType->stableKey.Equals("CClass"),
			*DumpMsg));
	}
};

#endif
