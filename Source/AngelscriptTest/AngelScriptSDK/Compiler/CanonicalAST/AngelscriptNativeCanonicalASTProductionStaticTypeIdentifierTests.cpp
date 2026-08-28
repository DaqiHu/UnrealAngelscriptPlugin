#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "source/as_generic.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused ProductionCodeGen slice: type identifier `CObj` must load
// `__StaticType_CObj` and pass that live class value to a native CClass formal.

static void* GProdStaticTypeValue = reinterpret_cast<void*>(static_cast<uintptr_t>(0x41));
static void* GProdStaticTypeObserved = nullptr;
static int32 GProdStaticTypeObserveCount = 0;

static void ProdObserveStaticType(asIScriptGeneric* Generic)
{
	++GProdStaticTypeObserveCount;
	GProdStaticTypeObserved = Generic != nullptr ? Generic->GetArgObject(0) : nullptr;
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionStaticTypeIdentifierTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.StaticTypeIdentifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TypeIdentifierArgumentPassesStaticTypeGlobal)
	{
		GProdStaticTypeObserved = nullptr;
		GProdStaticTypeObserveCount = 0;
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
				"CClass __StaticType_CObj", &GProdStaticTypeValue) >= 0,
			TEXT("__StaticType_CObj host global must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Observe(CClass Cls)",
				asFUNCTION(ProdObserveStaticType), asCALL_GENERIC) >= 0,
			TEXT("Observe(CClass) must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("static-type identifier fixture selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule(
			"ProdStaticTypeIdentifier", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				Observe(CObj);
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdStaticTypeIdentifier.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL must publish type-identifier-as-value without LEGACY fallback")));

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
			*FString::Printf(TEXT("must publish int F(); ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("static-type identifier must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("static-type identifier must never call the retained LEGACY compiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_CALLSYS),
			TEXT("the sealed Observe(CObj) call must remain a CALLSYS")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 1,
			TEXT("F() must execute after Canonical CodeGen")));
		ASSERT_THAT(AreEqual(1, GProdStaticTypeObserveCount,
			TEXT("Observe must run once")));
		ASSERT_THAT(IsTrue(
			GProdStaticTypeObserved == GProdStaticTypeValue,
			TEXT("Observe(CObj) must receive the live __StaticType_CObj class value")));
	}
};

#endif
