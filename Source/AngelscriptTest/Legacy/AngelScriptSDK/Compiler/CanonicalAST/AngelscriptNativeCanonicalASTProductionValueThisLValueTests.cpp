#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCanonicalASTTestSupport.h"
#include "../../Support/AngelscriptNativeCompilerBytecodeTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_dump.h"
#include "as_generic.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

// Focused ProductionCodeGen slice for VALUE-struct `this` passed to ?&.
// Prefix stays under Compiler.CanonicalAST.ProductionCodeGen.

static int32 GProdValueThisObserved = 0;
static int32 GProdValueThisObserveCount = 0;

static void ProdObserveValueThis(asIScriptGeneric* Generic)
{
	++GProdValueThisObserveCount;
	if (Generic == nullptr)
	{
		return;
	}
	int32* const Value = static_cast<int32*>(Generic->GetArgAddress(0));
	GProdValueThisObserved = Value != nullptr ? *Value : -1;
	if (Value != nullptr)
	{
		*Value = 41;
	}
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionValueThisLValueTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.ValueThisLValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValueStructThisToWildcardRefPassesLiveObjectAddress)
	{
		GProdValueThisObserved = 0;
		GProdValueThisObserveCount = 0;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterGlobalFunction(
			"void Observe(?& Value)",
			asFUNCTION(ProdObserveValueThis),
			asCALL_GENERIC) >= 0,
			TEXT("value-this lvalue observer must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("value-this lvalue fixture selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule(
			"ProdValueThisLValue", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FHost
			{
				int Value;

				int Touch()
				{
					Observe(this);
					return Value;
				}
			}

			int F()
			{
				FHost Host;
				Host.Value = 7;
				return Host.Touch();
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdValueThisLValue.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL must publish VALUE-struct this to ?& without LEGACY fallback")));

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
		asITypeInfo* const HostType = Module->GetTypeInfoByName("FHost");
		asIScriptFunction* const Touch =
			AngelscriptNativeTestSupport::GetNativeMethodByDecl(HostType, "int Touch()");
		ASSERT_THAT(IsNotNull(
			Function,
			*FString::Printf(TEXT("must publish int F(); ast {%s}"), *AstDump)));
		ASSERT_THAT(IsNotNull(
			Touch,
			*FString::Printf(TEXT("must publish int FHost::Touch(); ast {%s}"), *AstDump)));
		ASSERT_THAT(AreEqual(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN,
			InternalModule->GetLastBytecodePublisher(),
			TEXT("value-this lvalue must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("value-this lvalue must never call the retained LEGACY compiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Touch, asBC_CALLSYS),
			TEXT("the sealed Observe(this) call must remain a CALLSYS")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 41,
			TEXT("Observe(this) must write through the live FHost object, not a copy")));
		ASSERT_THAT(AreEqual(1, GProdValueThisObserveCount,
			TEXT("Observe must run once")));
		ASSERT_THAT(AreEqual(7, GProdValueThisObserved,
			TEXT("?& must receive the live FHost lvalue with Value=7")));
	}
};

#endif
