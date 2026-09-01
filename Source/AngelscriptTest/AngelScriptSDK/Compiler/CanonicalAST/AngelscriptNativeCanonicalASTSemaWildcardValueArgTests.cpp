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

// Focused SemaAuthority slice: value-object argument to a generic const ?&
// formal. Prefix stays under Compiler.CanonicalAST.SemaAuthority.

namespace CanonicalASTSemaWildcardValueArgTest
{
	struct FPayload
	{
		int32 Value = 0;
	};

	static void ConstructPayload(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr && Generic->GetObject() != nullptr)
		{
			new (Generic->GetObject()) FPayload();
		}
	}

	static void ObserveWildcard(asIScriptGeneric*)
	{
	}

	static void ApplyFormatUnused(asIScriptGeneric*)
	{
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaWildcardValueArgTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.WildcardValueArg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValueObjectArgumentToConstWildcardRefRemainsSealedCall)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FPayload",
			sizeof(CanonicalASTSemaWildcardValueArgTest::FPayload),
			asOBJ_VALUE | asOBJ_POD
				| asGetTypeTraits<CanonicalASTSemaWildcardValueArgTest::FPayload>()) >= 0,
			TEXT("wildcard value-arg fixture type must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FPayload",
			"int Value",
			asOFFSET(CanonicalASTSemaWildcardValueArgTest::FPayload, Value)) >= 0,
			TEXT("wildcard value-arg fixture property must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FPayload",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(CanonicalASTSemaWildcardValueArgTest::ConstructPayload),
			asCALL_GENERIC) >= 0,
			TEXT("wildcard value-arg fixture constructor must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"void Observe(const ?&in Value)",
			asFUNCTION(CanonicalASTSemaWildcardValueArgTest::ObserveWildcard),
			asCALL_GENERIC) >= 0,
			TEXT("wildcard value-arg observer must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaWildcardValueArg");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaWildcardValueArg module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void F()
			{
				FPayload Payload;
				Payload.Value = 41;
				Observe(Payload);
			}
			)AS");
		Code.SetCode("SemaWildcardValueArg.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("value-object to const ?&in must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("value-object to const ?&in must seal without CodeGen")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("Observe(Payload) must seal a Call with value-object arg and wildcard-ref formal. dump:\n%s"),
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
		const asCExpr* const Argument = Context.GetExpr(Call->children[0]);
		ASSERT_THAT(IsNotNull(Argument, *DumpMsg));
		if (Argument == nullptr)
		{
			return;
		}
		const asCType* const ArgumentType = Context.GetType(Argument->type.type);
		ASSERT_THAT(IsTrue(
			ArgumentType != nullptr
				&& ArgumentType->stableKey.Equals("FPayload")
				&& !Argument->type.IsHandle(),
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
			*FString::Printf(
				TEXT("formal must be wildcard reference; kind=%d ref=%s handle=%s. dump:\n%s"),
				FormalType != nullptr ? static_cast<int>(FormalType->kind) : -1,
				Formal->type.IsReference() ? TEXT("yes") : TEXT("no"),
				Formal->type.IsHandle() ? TEXT("yes") : TEXT("no"),
				*Text)));
	}

	TEST_METHOD(EnumArgumentSelectsWildcardOverIntegerOverloads)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("EVis") >= 0,
			TEXT("enum wildcard-ranking fixture enum must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue(
			"EVis", "Collapsed", 1) >= 0,
			TEXT("enum wildcard-ranking fixture enumerator must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ApplyFormat(int Value, int Specifier)",
			asFUNCTION(CanonicalASTSemaWildcardValueArgTest::ApplyFormatUnused),
			asCALL_GENERIC) >= 0,
			TEXT("int ApplyFormat overload must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ApplyFormat(int64 Value, int Specifier)",
			asFUNCTION(CanonicalASTSemaWildcardValueArgTest::ApplyFormatUnused),
			asCALL_GENERIC) >= 0,
			TEXT("int64 ApplyFormat overload must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"int ApplyFormat(const ?&in Value, int Specifier)",
			asFUNCTION(CanonicalASTSemaWildcardValueArgTest::ApplyFormatUnused),
			asCALL_GENERIC) >= 0,
			TEXT("wildcard ApplyFormat overload must register")));
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SemaEnumWildcardApplyFormat");
		ASSERT_THAT(IsNotNull(Module, TEXT("SemaEnumWildcardApplyFormat module")));
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCScriptCode Code;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int FormatCollapsedName()
			{
				return ApplyFormat(EVis::Collapsed, 1);
			}
			)AS");
		Code.SetCode("SemaEnumWildcardApplyFormat.as", ScriptSource.c_str(), true);

		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
			TEXT("enum ApplyFormat fixture must parse")));
		ASSERT_THAT(AreEqual(0, Context.Seal(),
			TEXT("enum ApplyFormat fixture must seal without CodeGen")));

		asCString Dump;
		asCASTDump(Context, Dump);
		const FString Text = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		const FString DumpMsg = FString::Printf(
			TEXT("enum must select const ?&in, not integer overloads. dump:\n%s"),
			*Text);

		const asCDecl* Callee = nullptr;
		for (asUINT ExprIndex = 1; ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL)
			{
				continue;
			}
			const asCDecl* const Target = Context.GetDecl(Expr->resolvedDecl);
			if (Target != nullptr && Target->name.Equals("ApplyFormat"))
			{
				Callee = Target;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Callee, *DumpMsg));
		if (Callee == nullptr)
		{
			return;
		}

		bool bSawWildcard = false;
		asUINT FormalCount = 0;
		for (asUINT ChildIndex = 0; ChildIndex < Callee->children.GetLength(); ++ChildIndex)
		{
			const asCDecl* const Param = Context.GetDecl(Callee->children[ChildIndex]);
			if (Param == nullptr || Param->kind != asAST_DECL_PARAM)
			{
				continue;
			}
			++FormalCount;
			if (FormalCount != 1)
			{
				continue;
			}
			const asCType* const FormalType = Context.GetType(Param->type.type);
			bSawWildcard = FormalType != nullptr
				&& FormalType->kind == asAST_TYPE_WILDCARD;
		}
		ASSERT_THAT(IsTrue(bSawWildcard, *DumpMsg));
	}
};

#endif
