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

// Focused SemaAuthority slice: a VALUE argument is viable for a REF formal when
// the VALUE type has zero-arg `opImplConv` to that formal. Prefix stays under
// Compiler.CanonicalAST.SemaAuthority.

namespace CanonicalASTSemaImplConvCallTest
{
	struct FHost
	{
		int32 Value = 0;
	};

	static void ConstructHost(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			new (Generic->GetObject()) FHost();
		}
	}

	static void ConvertHost(asIScriptGeneric*)
	{
	}

	static void ObserveClass(asIScriptGeneric*)
	{
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaImplConvCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.ImplConvCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValueOpImplConvMakesRefFormalCallViable)
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
				"FHost",
				sizeof(CanonicalASTSemaImplConvCallTest::FHost),
				asOBJ_VALUE | asOBJ_POD
					| asGetTypeTraits<CanonicalASTSemaImplConvCallTest::FHost>()) >= 0,
			TEXT("FHost VALUE type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FHost",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(CanonicalASTSemaImplConvCallTest::ConstructHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost constructor must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"FHost",
				"CClass opImplConv() const",
				asFUNCTION(CanonicalASTSemaImplConvCallTest::ConvertHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost opImplConv to CClass must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Observe(CClass Cls)",
				asFUNCTION(CanonicalASTSemaImplConvCallTest::ObserveClass),
				asCALL_GENERIC) >= 0,
			TEXT("Observe(CClass) must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaImplConvCall");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaImplConvCall module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				FHost Host;
				Observe(Host);
			}
			)AS");
		Code.SetCode("SemaImplConvCall.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("VALUE opImplConv to REF formal must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("VALUE opImplConv to REF formal must seal without CodeGen")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Observe(Host) must resolve via FHost::opImplConv to CClass. dump:\n%s"),
			*Text);

		const asCExpr* Call = nullptr;
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
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Call, *DumpMsg));
		if (Call == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Call->children.GetLength() >= 1, *DumpMsg));
		const asCExpr* Argument = Context.GetExpr(Call->children[0]);
		ASSERT_THAT(IsNotNull(Argument, *DumpMsg));
		if (Argument == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			Argument->kind == asAST_EXPR_CONVERSION, *DumpMsg));
		const asCType* const ArgumentType = Context.GetType(Argument->type.type);
		ASSERT_THAT(IsTrue(
			ArgumentType != nullptr && ArgumentType->stableKey.Equals("CClass"),
			*DumpMsg));
		ASSERT_THAT(IsTrue(Argument->children.GetLength() >= 1, *DumpMsg));
		const asCExpr* const Inner = Context.GetExpr(Argument->children[0]);
		const asCType* const InnerType = Inner != nullptr
			? Context.GetType(Inner->type.type) : nullptr;
		ASSERT_THAT(IsTrue(
			InnerType != nullptr && InnerType->stableKey.Equals("FHost"),
			*DumpMsg));
	}
};

#endif
