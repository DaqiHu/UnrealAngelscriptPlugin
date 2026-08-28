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

// Focused ProductionCodeGen slice: VALUE `opImplConv` to a REF formal must
// execute the conversion, not pass the VALUE slot as a handle.

struct FProdImplConvHost
{
	int32 Value = 0;
};

static void* GProdImplConvClass = reinterpret_cast<void*>(static_cast<uintptr_t>(0x41));
static void* GProdImplConvObserved = nullptr;
static int32 GProdImplConvObserveCount = 0;
static int32 GProdImplConvConvertCount = 0;

static void ProdConstructImplConvHost(asIScriptGeneric* Generic)
{
	if (Generic != nullptr && Generic->GetObject() != nullptr)
	{
		new (Generic->GetObject()) FProdImplConvHost();
	}
}

static void ProdConvertImplConvHost(asIScriptGeneric* Generic)
{
	++GProdImplConvConvertCount;
	if (Generic != nullptr)
	{
		Generic->SetReturnObject(GProdImplConvClass);
	}
}

static void ProdObserveImplConvClass(asIScriptGeneric* Generic)
{
	++GProdImplConvObserveCount;
	GProdImplConvObserved = Generic != nullptr ? Generic->GetArgObject(0) : nullptr;
}

TEST_CLASS_WITH_FLAGS(FCanonicalASTProductionImplConvCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.CanonicalAST.ProductionCodeGen.ImplConvCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValueOpImplConvPassesConvertedHandleNotValueSlot)
	{
		GProdImplConvObserved = nullptr;
		GProdImplConvObserveCount = 0;
		GProdImplConvConvertCount = 0;
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
				sizeof(FProdImplConvHost),
				asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FProdImplConvHost>()) >= 0,
			TEXT("FHost VALUE type must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"FHost",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ProdConstructImplConvHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost constructor must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectMethod(
				"FHost",
				"CClass opImplConv() const",
				asFUNCTION(ProdConvertImplConvHost),
				asCALL_GENERIC) >= 0,
			TEXT("FHost opImplConv to CClass must register")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void Observe(CClass Cls)",
				asFUNCTION(ProdObserveImplConvClass),
				asCALL_GENERIC) >= 0,
			TEXT("Observe(CClass) must register")));
		ASSERT_THAT(IsTrue(ScriptEngine->SetCompilerPipeline(asCOMPILER_PIPELINE_CANONICAL),
			TEXT("opImplConv call fixture selects CANONICAL")));

		asIScriptModule* Module = ScriptEngine->GetModule(
			"ProdImplConvCall", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(0, Module->SetASTRetentionPolicy(asAST_RETAIN_SNAPSHOT)));
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int F()
			{
				FHost Host;
				Observe(Host);
				return 1;
			}
			)AS");
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"ProdImplConvCall.as",
			ScriptSource.c_str(),
			static_cast<unsigned int>(ScriptSource.size()))));
		ASSERT_THAT(AreEqual(0, Module->Build(),
			TEXT("CANONICAL must publish VALUE opImplConv to REF formal without LEGACY fallback")));

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
			TEXT("opImplConv call must be published by Canonical CodeGen")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(0),
			InternalModule->GetLastLegacyCompilerInvocationCount(),
			TEXT("opImplConv call must never call the retained LEGACY compiler")));

		int32 Value = 0;
		ASSERT_THAT(IsTrue(
			AngelscriptNativeTestSupport::CanonicalExecuteInt(
				*TestRunner, Engine.Get(), Module, "int F()", Value)
				&& Value == 1,
			TEXT("F() must execute after Canonical CodeGen")));
		ASSERT_THAT(AreEqual(1, GProdImplConvConvertCount,
			TEXT("opImplConv must run once")));
		ASSERT_THAT(AreEqual(1, GProdImplConvObserveCount,
			TEXT("Observe must run once")));
		ASSERT_THAT(IsTrue(
			GProdImplConvObserved == GProdImplConvClass,
			TEXT("Observe must receive the converted handle, not the VALUE slot")));
	}
};

#endif
