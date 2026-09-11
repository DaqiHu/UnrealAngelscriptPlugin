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

// Focused ProductionCodeGen slice for value-object arguments to const ?&in.
// Prefix stays under Compiler.CanonicalAST.ProductionCodeGen.

struct FProdWildcardPayload
{
	int32 Value = 0;
};

static int32 GProdWildcardObserved = 0;
static int32 GProdWildcardObserveCount = 0;
static int32 GProdWildcardObservedTypeId = asTYPEID_VOID;

static void ProdConstructWildcardPayload(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		new (Generic->GetObject()) FProdWildcardPayload();
	}
}

static void ProdObserveWildcardPayload(asIScriptGeneric* Generic)
{
	++GProdWildcardObserveCount;
	if (Generic == nullptr)
	{
		return;
	}
	const FProdWildcardPayload* const Payload =
		static_cast<const FProdWildcardPayload*>(Generic->GetArgAddress(0));
	GProdWildcardObserved = Payload != nullptr ? Payload->Value : -1;
	GProdWildcardObservedTypeId = Generic->GetArgTypeId(0);
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionWildcardValueArgTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.WildcardValueArg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PreparedValueObjectToConstWildcardRefPassesLValueAddress)
	{
		GProdWildcardObserved = 0;
		GProdWildcardObserveCount = 0;
		GProdWildcardObservedTypeId = asTYPEID_VOID;
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType(
			"FPayload",
			sizeof(FProdWildcardPayload),
			asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FProdWildcardPayload>()) >= 0,
			TEXT("wildcard value-arg CodeGen type must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty(
			"FPayload",
			"int Value",
			asOFFSET(FProdWildcardPayload, Value)) >= 0,
			TEXT("wildcard value-arg CodeGen property must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"FPayload",
			asBEHAVE_CONSTRUCT,
			"void f()",
			asFUNCTION(ProdConstructWildcardPayload),
			asCALL_GENERIC) >= 0,
			TEXT("wildcard value-arg CodeGen constructor must register")));
		const int ObserveId = ScriptEngine->RegisterGlobalFunction(
			"void Observe(const ?&in Value)",
			asFUNCTION(ProdObserveWildcardPayload),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(ObserveId > 0,
			TEXT("wildcard value-arg observer must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("wildcard value-arg fixture selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule(
			"ProdWildcardValueArg", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FPayload Payload;
				Payload.Value = 41;
				Observe(Payload);
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdWildcardValueArg.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL must publish value-object to const ?&in without LEGACY fallback")));

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
			TEXT("wildcard value-arg must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("wildcard value-arg must never call the retained LEGACY compiler")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_CALLSYS),
			TEXT("the sealed Observe call must remain a CALLSYS")));
		ASSERT_THAT(IsTrue(
			AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Function, asBC_TYPEID),
			TEXT("the wildcard ABI must carry a late-patched TYPEID operand")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 1,
			TEXT("F() must execute after Canonical CodeGen")));
		ASSERT_THAT(AreEqual(1, GProdWildcardObserveCount,
			TEXT("Observe must run once")));
		ASSERT_THAT(AreEqual(41, GProdWildcardObserved,
			TEXT("const ?&in must receive the live FPayload lvalue, not a scalar slice")));
		ASSERT_THAT(AreEqual(
			ScriptEngine->GetTypeIdByDecl("FPayload"),
			GProdWildcardObservedTypeId,
			TEXT("GetArgTypeId must observe the current Engine-local FPayload projection")));
	}
};

#endif
