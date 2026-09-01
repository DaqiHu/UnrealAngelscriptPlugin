#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_decl.h"
#include "source/as_expr.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// CTA-S174 ports the historical TypedSemanticIR CallRewrite oracle onto the
// Canonical AST authority. These registration dispositions are source-language
// rewrites: discarded operands must not be resolved or evaluated, and no
// backend may rediscover the rewrite from a live Runtime function pointer.
namespace CanonicalASTCompileOutCallRewriteTest
{
	inline int32 NormalInvocationCount = 0;
	inline int32 ErasedInvocationCount = 0;
	inline int32 ReplacedInvocationCount = 0;
	inline int32 MethodChainInvocationCount = 0;

	static int32 InvokeNormal(const int32 Value)
	{
		++NormalInvocationCount;
		return Value + 3;
	}

	static void InvokeErased(const int32)
	{
		++ErasedInvocationCount;
	}

	static int32 InvokeReplaced(const int32 First, const int32)
	{
		++ReplacedInvocationCount;
		return First + 1000;
	}

	struct FChainProbe
	{
		int32 Value = 0;

		FChainProbe& Chain(const int32)
		{
			++MethodChainInvocationCount;
			return *this;
		}
	};

	static void ConstructChainProbe(FChainProbe* const Self)
	{
		new (Self) FChainProbe();
	}

	struct FRegisteredRewriteFunctions
	{
		int Normal = asERROR;
		int Erased = asERROR;
		int Replaced = asERROR;
		int MethodChain = asERROR;

		bool IsValid() const
		{
			return Normal >= 0 && Erased >= 0 && Replaced >= 0
				&& MethodChain >= 0;
		}
	};

	static FRegisteredRewriteFunctions RegisterRewriteFunctions(
		asIScriptEngine& ScriptEngine)
	{
		FRegisteredRewriteFunctions Result;
		const ASAutoCaller::FunctionCaller NormalCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeNormal);
		const ASAutoCaller::FunctionCaller ErasedCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeErased);
		const ASAutoCaller::FunctionCaller ReplacedCaller =
			ASAutoCaller::MakeFunctionCaller(InvokeReplaced);
		Result.Normal = ScriptEngine.RegisterGlobalFunction(
			"int RewriteNormal(int Value)",
			asFUNCTION(InvokeNormal),
			asCALL_CDECL,
			*(asFunctionCaller*)&NormalCaller);
		Result.Erased = ScriptEngine.RegisterGlobalFunction(
			"void RewriteErased(int Value)",
			asFUNCTION(InvokeErased),
			asCALL_CDECL,
			*(asFunctionCaller*)&ErasedCaller);
		Result.Replaced = ScriptEngine.RegisterGlobalFunction(
			"int RewriteReplaced(int First, int Ignored)",
			asFUNCTION(InvokeReplaced),
			asCALL_CDECL,
			*(asFunctionCaller*)&ReplacedCaller);

		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructChainProbe);
		const ASAutoCaller::FunctionCaller MethodCaller =
			ASAutoCaller::MakeFunctionCaller(&FChainProbe::Chain);
		if (ScriptEngine.RegisterObjectType(
				"FCallRewriteChainProbe",
				sizeof(FChainProbe),
				asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FChainProbe>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FCallRewriteChainProbe",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructChainProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0
			&& ScriptEngine.RegisterObjectProperty(
				"FCallRewriteChainProbe",
				"int Value",
				asOFFSET(FChainProbe, Value)) >= 0)
		{
			Result.MethodChain = ScriptEngine.RegisterObjectMethod(
				"FCallRewriteChainProbe",
				"FCallRewriteChainProbe& Chain(int Ignored)",
				asMETHODPR(FChainProbe, Chain, (int32), FChainProbe&),
				asCALL_THISCALL,
				*(asFunctionCaller*)&MethodCaller);
		}

		asCScriptFunction* const ErasedFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.Erased));
		asCScriptFunction* const ReplacedFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.Replaced));
		asCScriptFunction* const MethodChainFunction =
			static_cast<asCScriptFunction*>(
				ScriptEngine.GetFunctionById(Result.MethodChain));
		if (ErasedFunction != nullptr)
		{
			ErasedFunction->compileOutType =
				asECompileOutType::CompileOutEntirely;
		}
		if (ReplacedFunction != nullptr)
		{
			ReplacedFunction->compileOutType =
				asECompileOutType::ReplaceWithFirstParam;
		}
		if (MethodChainFunction != nullptr)
		{
			MethodChainFunction->compileOutType =
				asECompileOutType::CompileOutAsMethodChain;
		}
		return Result;
	}

	static void ResetCounters()
	{
		NormalInvocationCount = 0;
		ErasedInvocationCount = 0;
		ReplacedInvocationCount = 0;
		MethodChainInvocationCount = 0;
	}

	static std::string ScriptSource()
	{
		return ASTEST_AS_ANSI(R"AS(
			int ScalarRewriteEntry(int Value)
			{
				RewriteErased(MissingErasedArgument);
				return RewriteReplaced(
					Value + 1,
					MissingReplacementArgument)
					+ RewriteNormal(2);
			}

			int MethodChainRewriteEntry()
			{
				FCallRewriteChainProbe Value;
				Value.Value = 37;
				return Value.Chain(MissingMethodChainArgument).Value;
			}
			)AS");
	}

	static asIScriptModule* BuildModule(
		asCScriptEngine* const ScriptEngine,
		const char* const ModuleName,
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
			"CompileOutCallRewrite.as",
			Source.c_str(),
			static_cast<unsigned int>(Source.size())) < 0)
		{
			return Module;
		}
		OutBuildResult = Module->Build();
		return Module;
	}

	static const asCExpr* FindRewriteMarker(
		const asCASTContext& Context,
		const char* const Marker)
	{
		const asCExpr* Match = nullptr;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context.GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr =
				Context.GetExpr(asASTExprId(ExprIndex));
			if (Expr != nullptr && Expr->literal.Equals(Marker))
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Expr;
			}
		}
		return Match;
	}

	static int ExecuteGlobalInt(
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Function,
		const bool bSetArgument,
		const int32 Argument,
		int32& OutValue)
	{
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (Context == nullptr)
		{
			return asERROR;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (Context->Prepare(&Function) < 0
			|| (bSetArgument
				&& Context->SetArgDWord(
					0, static_cast<asDWORD>(Argument)) < 0))
		{
			return asERROR;
		}
		const int State = Context->Execute();
		if (State == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		return State;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTSemaCompileOutCallRewriteTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.SemaAuthority.CompileOutCallRewrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompilerRewritesSealFinalValuesWithoutExecutableCalls)
	{
		using namespace CanonicalASTCompileOutCallRewriteTest;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine != nullptr
			&& ScriptEngine->SetCompilerPipeline(
				asCOMPILER_PIPELINE_CANONICAL),
			TEXT("compile-out rewrite fixture selects CANONICAL")));
		const FRegisteredRewriteFunctions Registered =
			RegisterRewriteFunctions(*ScriptEngine);
		ASSERT_THAT(IsTrue(Registered.IsValid(),
			TEXT("compile-out rewrite fixture registers all dispositions")));

		int BuildResult = asERROR;
		asIScriptModule* const Module = BuildModule(
			ScriptEngine, "SemaCompileOutCallRewrites", BuildResult);
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
				TEXT("discarded operands must not produce diagnostics; build=%d messages=%s dump:\n%s"),
				BuildResult, *Messages, *Dump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal != nullptr
				? Internal->GetLastBytecodePublisher()
				: asBYTECODE_PUBLISHER_NONE,
			TEXT("compile-out rewrites publish through Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal != nullptr
				? Internal->GetLastLegacyCompilerInvocationCount()
				: 1u,
			TEXT("compile-out rewrites must not construct asCCompiler")));
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}

		const asCExpr* const Erased =
			FindRewriteMarker(*Context, "compile-out-entirely");
		const asCExpr* const Replaced =
			FindRewriteMarker(*Context, "compile-out-first-param");
		const asCExpr* const MethodChain =
			FindRewriteMarker(*Context, "compile-out-method-chain");
		const asCType* const ErasedType = Erased != nullptr
			? Context->GetType(Erased->type.type) : nullptr;
		const asCDecl* const ErasedCallee = Erased != nullptr
			? Context->GetDecl(Erased->resolvedDecl) : nullptr;
		const asCDecl* const ReplacedCallee = Replaced != nullptr
			? Context->GetDecl(Replaced->resolvedDecl) : nullptr;
		const asCDecl* const MethodChainCallee = MethodChain != nullptr
			? Context->GetDecl(MethodChain->resolvedDecl) : nullptr;
		const asDWORD CompileOutMask =
			asAST_TRAIT_COMPILE_OUT_ENTIRELY
			| asAST_TRAIT_REPLACE_WITH_FIRST_PARAM
			| asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN;
		ASSERT_THAT(IsTrue(
			Erased != nullptr
				&& Erased->kind == asAST_EXPR_CALL_REWRITE
				&& ErasedCallee != nullptr
				&& (ErasedCallee->traits & CompileOutMask)
					== asAST_TRAIT_COMPILE_OUT_ENTIRELY
				&& ErasedType != nullptr
				&& ErasedType->kind == asAST_TYPE_VOID
				&& Erased->children.GetLength() == 0
				&& Erased->callArguments.GetLength() == 0
				&& !Erased->receiver.IsValid(),
			*FString::Printf(
				TEXT("CompileOutEntirely must seal one typed CallRewrite void marker; dump:\n%s"),
					*Dump)));
		ASSERT_THAT(IsTrue(
			Replaced != nullptr
				&& Replaced->kind == asAST_EXPR_CALL_REWRITE
				&& ReplacedCallee != nullptr
				&& (ReplacedCallee->traits & CompileOutMask)
					== asAST_TRAIT_REPLACE_WITH_FIRST_PARAM
				&& !Replaced->receiver.IsValid()
				&& Replaced->children.GetLength() == 1
				&& Replaced->callArguments.GetLength() == 1
				&& Replaced->callArguments[0].expression
					== Replaced->children[0]
				&& Replaced->callArguments[0].formal.IsValid()
				&& Replaced->callArguments[0].formalIndex == 0
				&& Replaced->callArguments[0].sourceOrdinal == 0
				&& Replaced->callArguments[0].origin
					== asAST_CALL_ARGUMENT_POSITIONAL,
			*FString::Printf(
				TEXT("ReplaceWithFirstParam must retain exact first-value provenance; dump:\n%s"),
					*Dump)));
		ASSERT_THAT(IsTrue(
			MethodChain != nullptr
				&& MethodChain->kind == asAST_EXPR_CALL_REWRITE
				&& MethodChainCallee != nullptr
				&& MethodChainCallee->kind == asAST_DECL_METHOD
				&& (MethodChainCallee->traits & CompileOutMask)
					== asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN
				&& MethodChain->receiver.IsValid()
				&& MethodChain->children.GetLength() == 1
				&& MethodChain->children[0] == MethodChain->receiver
				&& MethodChain->callArguments.GetLength() == 1
				&& MethodChain->callArguments[0].expression
					== MethodChain->receiver
				&& !MethodChain->callArguments[0].formal.IsValid()
				&& MethodChain->callArguments[0].formalIndex
					== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				&& MethodChain->callArguments[0].sourceOrdinal
					== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				&& MethodChain->callArguments[0].origin
					== asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER,
			*FString::Printf(
				TEXT("CompileOutAsMethodChain must retain exact receiver provenance; dump:\n%s"),
					*Dump)));

		bool bSawNormalCall = false;
		bool bSawExecutableRewriteCall = false;
		for (asUINT ExprIndex = 1;
			ExprIndex <= Context->GetExprCount(); ++ExprIndex)
		{
			const asCExpr* const Expr =
				Context->GetExpr(asASTExprId(ExprIndex));
			const asCDecl* const Callee = Expr != nullptr
				? Context->GetDecl(Expr->resolvedDecl) : nullptr;
			if (Expr == nullptr || Expr->kind != asAST_EXPR_CALL
				|| Callee == nullptr)
			{
				continue;
			}
			bSawNormalCall = bSawNormalCall
				|| Callee->name.Equals("RewriteNormal");
			bSawExecutableRewriteCall = bSawExecutableRewriteCall
				|| Callee->name.Equals("RewriteErased")
				|| Callee->name.Equals("RewriteReplaced")
				|| Callee->name.Equals("Chain");
		}
		ASSERT_THAT(IsTrue(bSawNormalCall,
			TEXT("CompileCalls must remain one executable canonical Call")));
		ASSERT_THAT(IsFalse(bSawExecutableRewriteCall,
			*FString::Printf(
				TEXT("compiled-out dispositions must never remain executable Calls; dump:\n%s"),
				*Dump)));
	}
};

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionCompileOutCallRewriteTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.CompileOutCallRewrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompilerRewritesExecuteOnlyRetainedFinalValues)
	{
		using namespace CanonicalASTCompileOutCallRewriteTest;
		ResetCounters();
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine != nullptr
			&& ScriptEngine->SetCompilerPipeline(
				asCOMPILER_PIPELINE_CANONICAL),
			TEXT("production compile-out fixture selects CANONICAL")));
		const FRegisteredRewriteFunctions Registered =
			RegisterRewriteFunctions(*ScriptEngine);
		ASSERT_THAT(IsTrue(Registered.IsValid()));

		int BuildResult = asERROR;
		asIScriptModule* const Module = BuildModule(
			ScriptEngine, "ProductionCompileOutCallRewrites", BuildResult);
		const FString Messages =
			AngelscriptNativeTestSupport::CollectMessages(Engine.GetMessages());
		ASSERT_THAT(AreEqual(0, BuildResult,
			*FString::Printf(
				TEXT("compile-out execution fixture must build; build=%d messages=%s"),
				BuildResult, *Messages)));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}

		asCModule* const Internal = static_cast<asCModule*>(Module);
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			Internal->GetLastBytecodePublisher(),
			TEXT("compile-out execution Bytecode comes from Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			Internal->GetLastLegacyCompilerInvocationCount(),
			TEXT("compile-out execution must not invoke LEGACY")));

		asIScriptFunction* const ScalarEntry = Module->GetFunctionByDecl(
			"int ScalarRewriteEntry(int Value)");
		asIScriptFunction* const MethodChainEntry = Module->GetFunctionByDecl(
			"int MethodChainRewriteEntry()");
		ASSERT_THAT(IsNotNull(ScalarEntry));
		ASSERT_THAT(IsNotNull(MethodChainEntry));
		if (ScalarEntry == nullptr || MethodChainEntry == nullptr)
		{
			return;
		}

		int32 ScalarValue = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteGlobalInt(
				*ScriptEngine, *ScalarEntry, true, 10, ScalarValue)));
		ASSERT_THAT(AreEqual(16, ScalarValue,
			TEXT("first-parameter rewrite plus ordinary native call is exact")));
		int32 MethodChainValue = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteGlobalInt(
				*ScriptEngine, *MethodChainEntry, false, 0,
				MethodChainValue)));
		ASSERT_THAT(AreEqual(37, MethodChainValue));
		ASSERT_THAT(AreEqual(1, NormalInvocationCount));
		ASSERT_THAT(AreEqual(0, ErasedInvocationCount));
		ASSERT_THAT(AreEqual(0, ReplacedInvocationCount));
		ASSERT_THAT(AreEqual(0, MethodChainInvocationCount));
	}
};

#endif
