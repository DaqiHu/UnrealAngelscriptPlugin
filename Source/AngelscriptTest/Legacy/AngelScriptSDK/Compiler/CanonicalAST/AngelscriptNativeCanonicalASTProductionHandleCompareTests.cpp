#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused ProductionCodeGen slices live here so
// AngelscriptNativeCanonicalASTProductionCodeGenTests.cpp does not keep growing.
// Prefix stays under Compiler.CanonicalAST.ProductionCodeGen so the AST-first
// gate command still discovers these methods.

static void ProdHandleCompareAddRef(asIScriptGeneric*)
{
}

static void ProdHandleCompareRelease(asIScriptGeneric*)
{
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionHandleCompareTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.HandleCompare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CanonicalHandleInequalityEmitsCmpPtrNotCMPi)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType("CObj", 0, asOBJ_REF) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CObj", asBEHAVE_ADDREF, "void f()",
				asFUNCTION(ProdHandleCompareAddRef), asCALL_GENERIC) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"CObj", asBEHAVE_RELEASE, "void f()",
				asFUNCTION(ProdHandleCompareRelease), asCALL_GENERIC) >= 0,
			TEXT("handle inequality fixture registers a host REF type")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("handle inequality fixture selects CANONICAL")));
		asIScriptModule* Module = ScriptEngine->GetModule(
			"ProdHandleInequality", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("handle inequality fixture needs a module")));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT),
			TEXT("retain sealed AST so opcode provenance can name the Binary !=")));
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
			"ProdHandleInequality",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size())),
			TEXT("handle inequality fixture must add the section")));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL handle inequality Build must succeed from CodeGen")));

		asCModule* const InternalModule = static_cast<asCModule*>(Module);
		FString AstDump = TEXT("<no retained AST>");
		if (const asCASTContext* const Sealed = InternalModule->GetCanonicalASTContext())
		{
			asCString Dump;
			asCASTDump(*Sealed, Dump);
			AstDump = AngelscriptNativeTestSupport::CanonicalAstDumpToFString(Dump);
		}
		asIScriptFunction* const Function =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int F()");
		ASSERT_THAT(IsNotNull(
			Function,
			*FString::Printf(
				TEXT("CANONICAL handle inequality must publish int F(); ast {%s}"),
				*AstDump)));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_CmpPtr),
			*FString::Printf(
				TEXT("handle != must emit CmpPtr, not integer CMPi. ast {%s}"),
				*AstDump)));
		ASSERT_THAT(IsFalse(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_CMPi),
			TEXT("handle != must not fall through to CMPi")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("handle inequality must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("handle inequality must never call the retained LEGACY compiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 0,
			TEXT("null != null must be false through CmpPtr")));
	}
};

#endif
