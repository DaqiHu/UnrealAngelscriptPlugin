#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "source/as_module.h"
#include "source/as_restore.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeTypedSemanticIRArchiveIsolationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ArchiveIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExecuteArchiveEntry(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		const int32 Input,
		int32& OutValue)
	{
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return false;
		}
		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (Context->Prepare(Function) < 0
			|| Context->SetArgDWord(0, static_cast<asDWORD>(Input)) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

	static bool WriteFunctionArtifact(
		asIScriptModule* ScriptModule,
		asCScriptFunction* Function,
		AngelscriptNativeTestSupport::FMemoryBinaryStream& OutStream)
	{
		asCModule* const Module = static_cast<asCModule*>(ScriptModule);
		if (Module == nullptr || Function == nullptr)
		{
			return false;
		}
		asCWriter Writer(Module, &OutStream, Module->engine, true);
		return Writer.WriteFunctionArtifact(Function) == asSUCCESS
			&& OutStream.Num() > 0;
	}

public:
	TEST_METHOD(BytecodeAndFunctionArtifactsNeverPersistCapturedHir)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-ARCHIVE-ISOLATION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int TypedSemanticArchiveEntry(int Input)
			{
				int ArchiveOnlyLocal = Input * 3;
				return ArchiveOnlyLocal + 1;
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRArchive",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRArchive",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOffModule,
				"int TypedSemanticArchiveEntry(const int Input)"));
		asCScriptFunction* const CaptureOnFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				CaptureOnModule,
				"int TypedSemanticArchiveEntry(const int Input)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction()));
		ASSERT_THAT(IsNotNull(CaptureOnFunction->GetTypedSemanticFunction()));

		FMemoryBinaryStream CaptureOffBytecode;
		FMemoryBinaryStream CaptureOnBytecode;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			CaptureOffModule.Get()->SaveByteCode(&CaptureOffBytecode, false)));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			CaptureOnModule.Get()->SaveByteCode(&CaptureOnBytecode, false)));
		ASSERT_THAT(IsTrue(CaptureOffBytecode.Num() > 0));
		ASSERT_THAT(AreEqual(
			CaptureOffBytecode.GetBytes(),
			CaptureOnBytecode.GetBytes(),
			TEXT("SaveByteCode bytes must be identical because HIR is not serialized")));

		FMemoryBinaryStream CaptureOffFunctionArtifact;
		FMemoryBinaryStream CaptureOnFunctionArtifact;
		ASSERT_THAT(IsTrue(WriteFunctionArtifact(
			CaptureOffModule.Get(),
			CaptureOffFunction,
			CaptureOffFunctionArtifact)));
		ASSERT_THAT(IsTrue(WriteFunctionArtifact(
			CaptureOnModule.Get(),
			CaptureOnFunction,
			CaptureOnFunctionArtifact)));
		ASSERT_THAT(AreEqual(
			CaptureOffFunctionArtifact.GetBytes(),
			CaptureOnFunctionArtifact.GetBytes(),
			TEXT("Cache V2 function-artifact bytes must be identical because HIR is not serialized")));

		FNativeTestEngine RestoredEngine;
		RestoredEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			RestoredEngine.Destroy();
		};
		asCScriptEngine* const RestoredScriptEngine =
			static_cast<asCScriptEngine*>(RestoredEngine.Get());
		ASSERT_THAT(IsTrue(
			RestoredScriptEngine->SetTypedSemanticIRCapture(true)));
		asIScriptModule* const RestoredModule = RestoredScriptEngine->GetModule(
			"TypedSemanticIRArchiveRestored", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(RestoredModule));
		if (RestoredModule == nullptr)
		{
			return;
		}
		CaptureOnBytecode.ResetReadPosition();
		bool bDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			RestoredModule->LoadByteCode(
				&CaptureOnBytecode, &bDebugInfoStripped)));
		ASSERT_THAT(IsFalse(bDebugInfoStripped));
		asCScriptFunction* const RestoredFunction = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				RestoredModule,
				"int TypedSemanticArchiveEntry(const int Input)"));
		ASSERT_THAT(IsNotNull(RestoredFunction));
		if (RestoredFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNull(
			RestoredFunction->GetTypedSemanticFunction(),
			TEXT("A bytecode-only load must not reconstruct or persist HIR")));
		int32 RestoredValue = 0;
		ASSERT_THAT(IsTrue(ExecuteArchiveEntry(
			RestoredScriptEngine, RestoredFunction, 7, RestoredValue)));
		ASSERT_THAT(AreEqual(22, RestoredValue,
			TEXT("A bytecode-only load must retain ordinary VM behavior")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
