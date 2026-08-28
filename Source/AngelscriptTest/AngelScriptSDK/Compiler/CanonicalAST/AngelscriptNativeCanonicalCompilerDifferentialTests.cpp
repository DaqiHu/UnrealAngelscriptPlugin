#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalCompilerDifferentialTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.Differential",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LegacyPipelineStillCompiles)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_LEGACY),
			TEXT("legacy pipeline selection should succeed")));
		asIScriptModule* Module = nullptr;
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"LegacyDiff",
			"int F() { return 7; }",
			Module);
		ASSERT_THAT(AreEqual(0, BuildResult, TEXT("legacy pipeline should still compile")));
	}

	TEST_METHOD(CanonicalSelectionBuildsAndExecutes)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("canonical pipeline selection must exist")));
		ASSERT_THAT(AreEqual(asCOMPILER_PIPELINE_CANONICAL, ScriptEngine->GetCompilerPipeline(),
			TEXT("selected pipeline should be readable")));
		ASSERT_THAT(IsTrue(ScriptEngine->IsCanonicalBytecodeCodeGenReady(),
			TEXT("Ready is true because CANONICAL Build() calls Generate")));

		asIScriptModule* Module = nullptr;
		const int BuildResult = AngelscriptNativeTestSupport::CompileNativeModule(
			Engine.Get(),
			"CanonicalDiff",
			"int F() { return 7; }",
			Module);
		ASSERT_THAT(AreEqual(0, BuildResult, TEXT("canonical selection must compile after cutover")));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(*TestRunner, Engine.Get(), Module, "int F()", Value),
			TEXT("canonical Build() must produce executable bytecode")));
		ASSERT_THAT(AreEqual(7, Value, TEXT("canonical F() must return 7")));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			static_cast<asCModule*>(Module)->GetLastBytecodePublisher(),
			TEXT("CANONICAL Build of int F() must publish CodeGen")));
	}
};

#endif
