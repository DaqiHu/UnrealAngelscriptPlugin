#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeJitCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.JitCompiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FRecordingJitCompiler final : public asIJITCompiler
	{
	public:
		bool bPublishFunction = false;
		int32 CompileCount = 0;
		int32 ReleaseCount = 0;
		TArray<FString> CompiledDeclarations;
		TArray<asSJITFunctionBinding> ReleasedBindings;

		void OnFunctionReady(asIScriptFunction* Function) override
		{
			++CompileCount;
			CompiledDeclarations.Add(
				Function != nullptr && Function->GetDeclaration() != nullptr
					? UTF8_TO_TCHAR(Function->GetDeclaration())
					: TEXT("<null>"));
			if (bPublishFunction && Function != nullptr)
			{
				asSJITFunctionBinding Binding;
				Binding.VMEntry = &SentinelJitFunction;
				Function->SetJITBinding(Binding);
			}
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding& Binding) override
		{
			++ReleaseCount;
			ReleasedBindings.Add(Binding);
		}

		static void SentinelJitFunction(
			FScriptExecution&,
			asDWORD*,
			asQWORD*)
		{
		}
	};

	static std::string BuildSource(const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), Value));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return TCHAR_TO_UTF8(*Source);
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const ANSICHAR* ModuleName,
		const std::string& Source)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			UTF8_TO_TCHAR(ModuleName),
			UTF8_TO_TCHAR(Source.c_str()));
	}

public:
	TEST_METHOD(InstallCompileReplaceAndClear)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("EMBED-JIT-INSTALL-COMPILE-LIFECYCLE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("JIT lifecycle product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, 1),
			TEXT("JIT lifecycle product should explicitly retain JIT entry instructions")));
		FRecordingJitCompiler PrimaryCompiler;
		FRecordingJitCompiler ReplacementCompiler;
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(nullptr);
		};

		ASSERT_THAT(IsNull(ScriptEngine->GetJITCompiler(), TEXT("JIT lifecycle product should begin with no compiler installed")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&PrimaryCompiler), TEXT("JIT lifecycle product should install the primary compiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asIJITCompiler*>(&PrimaryCompiler),
			ScriptEngine->GetJITCompiler(),
			TEXT("JIT lifecycle product should return the exact installed primary compiler")));

		const std::string PrimarySource = BuildSource(11);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.install-primary"), "NativeJitPrimary", PrimarySource);
		{
			FScopedNativeModule PrimaryModule(*TestRunner, Engine, "NativeJitPrimary", PrimarySource);
			ASSERT_THAT(IsTrue(PrimaryModule.IsValid(), TEXT("JIT lifecycle product should compile with the primary compiler installed")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				PrimaryModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the primary interpreted entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					11,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Primary compiler with no published JIT function should preserve interpreted runtime behavior")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitPrimary", asGM_ONLY_IF_EXISTS),
			TEXT("Primary JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(IsTrue(PrimaryCompiler.CompileCount > 0, TEXT("JIT lifecycle product should route script functions through the primary compiler")));
		ASSERT_THAT(IsTrue(
			PrimaryCompiler.CompiledDeclarations.Contains(TEXT("int Entry()")),
			TEXT("JIT lifecycle product should report the exact primary script declaration")));
		const int32 PrimaryCompileCount = PrimaryCompiler.CompileCount;

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(&ReplacementCompiler), TEXT("JIT lifecycle product should replace the installed compiler")));
		ASSERT_THAT(AreEqual(
			static_cast<asIJITCompiler*>(&ReplacementCompiler),
			ScriptEngine->GetJITCompiler(),
			TEXT("JIT lifecycle product should return the exact replacement compiler")));

		const std::string ReplacementSource = BuildSource(22);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.install-replacement"), "NativeJitReplacement", ReplacementSource);
		{
			FScopedNativeModule ReplacementModule(*TestRunner, Engine, "NativeJitReplacement", ReplacementSource);
			ASSERT_THAT(IsTrue(ReplacementModule.IsValid(), TEXT("JIT lifecycle product should compile with the replacement compiler installed")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				ReplacementModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the replacement interpreted entry")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					22,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Replacement compiler with no published JIT function should preserve interpreted runtime behavior")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitReplacement", asGM_ONLY_IF_EXISTS),
			TEXT("Replacement JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(AreEqual(PrimaryCompileCount, PrimaryCompiler.CompileCount, TEXT("JIT lifecycle product should stop calling the replaced compiler")));
		ASSERT_THAT(IsTrue(ReplacementCompiler.CompileCount > 0, TEXT("JIT lifecycle product should route later functions through the replacement compiler")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetJITCompiler(nullptr), TEXT("JIT lifecycle product should clear the replacement compiler")));
		ASSERT_THAT(IsNull(ScriptEngine->GetJITCompiler(), TEXT("JIT lifecycle product should expose the cleared compiler state")));
		const int32 ReplacementCompileCount = ReplacementCompiler.CompileCount;

		const std::string ClearedSource = BuildSource(33);
		PrintSource(*TestRunner, TEXT("EMBED-JIT.cleared"), "NativeJitCleared", ClearedSource);
		{
			FScopedNativeModule ClearedModule(*TestRunner, Engine, "NativeJitCleared", ClearedSource);
			ASSERT_THAT(IsTrue(ClearedModule.IsValid(), TEXT("JIT lifecycle product should retain interpreted compilation after clearing the compiler")));
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				*TestRunner,
				ScriptEngine,
				ClearedModule,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				Invoker.IsValid(),
				TEXT("JIT lifecycle product should prepare the entry after clearing the compiler")));
			if (Invoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					33,
					Invoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Cleared compiler state should execute the exact interpreted entry")));
			}
		}
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("NativeJitCleared", asGM_ONLY_IF_EXISTS),
			TEXT("Cleared-state JIT module should be absent after scoped cleanup")));
		ASSERT_THAT(AreEqual(
			ReplacementCompileCount,
			ReplacementCompiler.CompileCount,
			TEXT("JIT lifecycle product should not call a compiler after clear")));
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
