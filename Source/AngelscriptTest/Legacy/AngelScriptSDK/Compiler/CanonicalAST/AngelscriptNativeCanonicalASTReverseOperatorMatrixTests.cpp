#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_ast_type.h"
#include "frontend/AST/as_decl.h"
#include "frontend/AST/as_expr.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Family-complete task-5.3 characterization: every non-comparison binary
// operator that LEGACY maps to an opX_r name must already arrive at CodeGen as
// an exact rhs-receiver Call. The matrix intentionally uses non-commutative
// spellings and unique return sentinels so no operator can be inferred from the
// single existing opAdd_r proof.

namespace CanonicalASTReverseOperatorMatrixTest
{
	struct FExpectedOperator
	{
		const char* MethodName;
		const char* StableKey;
		const char* AuthoredLeft;
	};

	static const FExpectedOperator ExpectedOperators[] =
	{
		{"opAdd_r", "T::opAdd_r(int) const", "1"},
		{"opSub_r", "T::opSub_r(int) const", "2"},
		{"opMul_r", "T::opMul_r(int) const", "3"},
		{"opDiv_r", "T::opDiv_r(int) const", "4"},
		{"opMod_r", "T::opMod_r(int) const", "5"},
		{"opPow_r", "T::opPow_r(int) const", "6"},
		{"opOr_r", "T::opOr_r(int) const", "7"},
		{"opAnd_r", "T::opAnd_r(int) const", "8"},
		{"opXor_r", "T::opXor_r(int) const", "9"},
		{"opShl_r", "T::opShl_r(int) const", "10"},
		{"opShr_r", "T::opShr_r(int) const", "11"},
		{"opUShr_r", "T::opUShr_r(int) const", "12"},
	};

	static constexpr int32 ExpectedExecutionSum = 78;

	static std::string ScriptSource()
	{
		return ASTEST_AS_ANSI(R"AS(
			struct T
			{
				int opAdd_r(int Value) const { return 1; }
				int opSub_r(int Value) const { return 2; }
				int opMul_r(int Value) const { return 3; }
				int opDiv_r(int Value) const { return 4; }
				int opMod_r(int Value) const { return 5; }
				int opPow_r(int Value) const { return 6; }
				int opOr_r(int Value) const { return 7; }
				int opAnd_r(int Value) const { return 8; }
				int opXor_r(int Value) const { return 9; }
				int opShl_r(int Value) const { return 10; }
				int opShr_r(int Value) const { return 11; }
				int opUShr_r(int Value) const { return 12; }
			}

			int Entry()
			{
				T Object;
				return (1 + Object)
					+ (2 - Object)
					+ (3 * Object)
					+ (4 / Object)
					+ (5 % Object)
					+ (6 ** Object)
					+ (7 | Object)
					+ (8 & Object)
					+ (9 ^ Object)
					+ (10 << Object)
					+ (11 >> Object)
					+ (12 >>> Object);
			}
			)AS");
	}

	static asIScriptModule* BuildModule(
		asCScriptEngine* ScriptEngine,
		const char* ModuleName,
		int& OutBuildResult)
	{
		OutBuildResult = asERROR;
		if (ScriptEngine == nullptr)
		{
			return nullptr;
		}
		asIScriptModule* const Module = ScriptEngine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE);
		if (Module == nullptr
			|| Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT) < 0)
		{
			return Module;
		}
		const std::string Source = ScriptSource();
		if (Module->AddScriptSection(
			"ReverseOperatorMatrix.as",
			Source.c_str(),
			static_cast<unsigned int>(Source.size())) < 0)
		{
			return Module;
		}
		OutBuildResult = Module->Build();
		return Module;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaReverseOperatorMatrixTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.ReverseOperatorMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AllLegacyReverseOperatorNamesSealExactRhsReceiverCalls)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine != nullptr
			&& ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("reverse-operator matrix selects CANONICAL")));
		int BuildResult = asERROR;
		asIScriptModule* const Module =
			CanonicalASTReverseOperatorMatrixTest::BuildModule(
				ScriptEngine, "SemaReverseOperatorMatrix", BuildResult);
		ASSERT_THAT(IsNotNull(Module));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		const asCASTContext* const Context = Internal != nullptr
			? Internal->GetCanonicalASTContext() : nullptr;
		asCString RawDump;
		if (Context != nullptr)
		{
			asCASTDump(*Context, RawDump);
		}
		const FString Dump =
			AngelscriptNativeTestSupport::CanonicalAstDumpToFString(RawDump);
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("all twelve reverse operators must build from sealed Calls; build=%d messages=%s dump:\n%s"),
				BuildResult,
				*Messages,
				*Dump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("reverse-operator matrix must publish through Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("reverse-operator matrix must not construct asCCompiler")));
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		bool Seen[UE_ARRAY_COUNT(
			CanonicalASTReverseOperatorMatrixTest::ExpectedOperators)] = {};
		bool bSawObjectTypedBinaryOperand = false;
		for (asUINT ExprIndex = 1; ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr = Context->GetExpr(asASTExprId(ExprIndex));
			if (Expr == nullptr)
			{
				continue;
			}
			if (Expr->kind == asAST_EXPR_BINARY)
			{
				for (asUINT ChildIndex = 0;
					ChildIndex < Expr->children.GetLength(); ++ChildIndex)
				{
					const asCExpr* const Child =
						Context->GetExpr(Expr->children[ChildIndex]);
					const asCType* const ChildType = Child != nullptr
						? Context->GetType(Child->type.type) : nullptr;
					bSawObjectTypedBinaryOperand = bSawObjectTypedBinaryOperand
						|| (ChildType != nullptr
							&& ChildType->stableKey.Equals("T"));
				}
			}

			const asCDecl* const Callee =
				Context->GetDecl(Expr->resolvedDecl);
			if (Expr->kind != asAST_EXPR_CALL || Callee == nullptr)
			{
				continue;
			}
			for (int32 ExpectedIndex = 0;
				ExpectedIndex < UE_ARRAY_COUNT(
					CanonicalASTReverseOperatorMatrixTest::ExpectedOperators);
				++ExpectedIndex)
			{
				const auto& Expected =
					CanonicalASTReverseOperatorMatrixTest::ExpectedOperators[
						ExpectedIndex];
				if (!Callee->stableKey.Equals(Expected.StableKey))
				{
					continue;
				}

				const asCExpr* const Receiver =
					Context->GetExpr(Expr->receiver);
				const asCDecl* const ReceiverDecl = Receiver != nullptr
					? Context->GetDecl(Receiver->resolvedDecl) : nullptr;
				const asSASTCallArgument* const Argument =
					Expr->callArguments.GetLength() == 1
						? &Expr->callArguments[0] : nullptr;
				const asCDecl* const Formal = Argument != nullptr
					? Context->GetDecl(Argument->formal) : nullptr;
				const asCExpr* const AuthoredLeft = Argument != nullptr
					? Context->GetExpr(Argument->expression) : nullptr;
				const asCType* const ResultType =
					Context->GetType(Expr->type.type);
				const FString Evidence = FString::Printf(
					TEXT("%s must be an exact rhs-receiver Call with the authored lhs in formal Value; dump:\n%s"),
					ANSI_TO_TCHAR(Expected.MethodName),
					*Dump);
				ASSERT_THAT(IsTrue(
					!Seen[ExpectedIndex]
						&& Expr->callDispatch == asAST_CALL_DISPATCH_DIRECT
						&& Receiver != nullptr
						&& ReceiverDecl != nullptr
						&& ReceiverDecl->name.Equals("Object")
						&& Argument != nullptr
						&& Argument->origin == asAST_CALL_ARGUMENT_POSITIONAL
						&& Argument->formalIndex == 0
						&& Formal != nullptr
						&& Formal->name.Equals("Value")
						&& AuthoredLeft != nullptr
						&& AuthoredLeft->kind == asAST_EXPR_INTEGER_LITERAL
						&& AuthoredLeft->literal.Equals(Expected.AuthoredLeft)
						&& Expr->children.GetLength() == 1
						&& Expr->children[0] == Argument->expression
						&& ResultType != nullptr
						&& ResultType->stableKey.Equals("int"),
					*Evidence));
				Seen[ExpectedIndex] = true;
			}
		}

		for (int32 ExpectedIndex = 0;
			ExpectedIndex < UE_ARRAY_COUNT(
				CanonicalASTReverseOperatorMatrixTest::ExpectedOperators);
			++ExpectedIndex)
		{
			ASSERT_THAT(IsTrue(Seen[ExpectedIndex],
				*FString::Printf(
					TEXT("missing sealed reverse Call %s; dump:\n%s"),
					ANSI_TO_TCHAR(
						CanonicalASTReverseOperatorMatrixTest::ExpectedOperators[
							ExpectedIndex].MethodName),
					*Dump)));
		}
		ASSERT_THAT(IsFalse(bSawObjectTypedBinaryOperand,
			*FString::Printf(
				TEXT("no reverse family may remain a Binary with an object operand; dump:\n%s"),
				*Dump)));
	}
};

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionReverseOperatorMatrixTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.ReverseOperatorMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AllLegacyReverseOperatorNamesExecuteWithoutLegacyCompiler)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine != nullptr
			&& ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("production reverse-operator matrix selects CANONICAL")));
		int BuildResult = asERROR;
		asIScriptModule* const Module =
			CanonicalASTReverseOperatorMatrixTest::BuildModule(
				ScriptEngine, "ProductionReverseOperatorMatrix", BuildResult);
		ASSERT_THAT(IsNotNull(Module));

		asCModule* const Internal = static_cast<asCModule*>(Module);
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("reverse-operator execution matrix must build; build=%d messages=%s"),
				BuildResult,
				*Messages)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("reverse-operator matrix Bytecode must come from Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("reverse-operator matrix must not construct asCCompiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int Entry()", Value)
				&& Value
					== CanonicalASTReverseOperatorMatrixTest::ExpectedExecutionSum,
			*FString::Printf(
				TEXT("all twelve reverse methods must execute exactly once; Entry()=%d expected=%d"),
				Value,
				CanonicalASTReverseOperatorMatrixTest::ExpectedExecutionSum)));
	}
};

#endif
