#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegatePayloadsTests
{
	TUniquePtr<FAngelscriptEngine> MakeHostEngine(FString& OutDiagnostic)
	{
		if (!FAngelscriptBind::PrepareForEngineInitialization(OutDiagnostic))
		{
			return {};
		}
		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bSkipInitialCompile = true;
		Config.bOverrideCacheV2Enablement = true;
		Config.bEnableCacheV2 = false;
		return FAngelscriptEngine::Create(Config, FAngelscriptEngineDependencies::CreateDefault());
	}

	TSharedRef<FAngelscriptModuleDesc> MakeModule(const TCHAR* Token, const FString& Code)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = FString::Printf(TEXT("DelegatePayloads.%s"), Token);
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.AbsoluteFilename = Module->ModuleName + TEXT(".as");
		Section.RelativeFilename = Section.AbsoluteFilename;
		Section.Code = Code;
		Module->Code.Add(MoveTemp(Section));
		return Module;
	}

	asIScriptFunction* FindGlobal(asIScriptEngine* Engine, const char* Name)
	{
		if (!Engine || !Name)
		{
			return nullptr;
		}
		const asUINT Count = Engine->GetGlobalFunctionCount();
		for (asUINT Index = 0; Index < Count; ++Index)
		{
			asIScriptFunction* Function = Engine->GetGlobalFunctionByIndex(Index);
			if (Function && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

	bool Compile(
		FAngelscriptEngine& Engine,
		const TCHAR* Token,
		const FString& Code,
		TSharedPtr<FAngelscriptModuleDesc>& OutModule,
		FString& OutError)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(MakeModule(Token, Code));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		const ECompileResult Result = Engine.CompileModules(ECompileType::Initial, InModules, OutModules);
		OutModule = InModules[0];
		if (Result != ECompileResult::FullyHandled && Result != ECompileResult::PartiallyHandled)
		{
			OutError = FString::Printf(
				TEXT("CompileModules=%d compileError=%d diagnostics=%s"),
				int32(Result),
				OutModule->bCompileError ? 1 : 0,
				*Engine.FormatDiagnostics());
			return false;
		}
		if (OutModule->bCompileError || OutModule->ScriptModule == nullptr)
		{
			OutError = FString::Printf(
				TEXT("compileError=%d scriptModule=%d diagnostics=%s"),
				OutModule->bCompileError ? 1 : 0,
				OutModule->ScriptModule ? 1 : 0,
				*Engine.FormatDiagnostics());
			return false;
		}
		return true;
	}

	int32 ExecuteInt(asIScriptEngine* ScriptEngine, asIScriptFunction* Function, FString& OutError)
	{
		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Context)
		{
			OutError = TEXT("CreateContext failed");
			return 0;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (Context->Prepare(Function) < 0)
		{
			OutError = TEXT("Prepare failed");
			return 0;
		}
		const int Executed = Context->Execute();
		if (Executed != asEXECUTION_FINISHED)
		{
			OutError = FString::Printf(TEXT("execute=%d exception=%s"), Executed, UTF8_TO_TCHAR(Context->GetExceptionString()));
			return 0;
		}
		return static_cast<int32>(Context->GetReturnDWord());
	}
}

TEST_CLASS_WITH_FLAGS(DelegatePayloads, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StoredOffset)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePayloadsTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePayloadsTests::Compile(*Engine, TEXT("StoredOffset"),
			TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FTransform, int);\n")
			TEXT("int AddOffset(int Value, int Offset) { return Value + Offset; }\n")
			TEXT("int Run() { FTransform Handler; Handler.Create(AddOffset, 10); return Handler.Execute(5); }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegatePayloadsTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegatePayloadsTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(15, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(FactoryLocalUnchanged)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePayloadsTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegatePayloadsTests::Compile(*Engine, TEXT("FactoryLocal"),
			TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FTransform, int);\n")
			TEXT("int AddOffset(int Value, int Offset) { return Value + Offset; }\n")
			TEXT("int Run() { int Offset = 10; FTransform Handler; Handler.Bind(AddOffset, Offset); Offset = 99; return Handler.Execute(5); }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegatePayloadsTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegatePayloadsTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(15, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(RejectStackRefPayload)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePayloadsTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(DelegatePayloadsTests::MakeModule(
			TEXT("StackRef"),
			TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FTransform, int);\n")
			TEXT("int AddOffset(int Value, int& Offset) { return Value + Offset; }\n")
			TEXT("int Run() { int Offset = 10; FTransform Handler; Handler.Bind(AddOffset, Offset); return Handler.Execute(5); }\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("Stage 5"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		(void)Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		ASSERT_THAT(IsTrue(InModules[0]->bCompileError));
		const FString Combined = Engine->FormatDiagnostics();
		ASSERT_THAT(IsTrue(
			Combined.Contains(TEXT("escaping-stack-ref-payload"))
				|| Combined.Contains(TEXT("4459")),
			*Combined));
	}

	TEST_METHOD(LambdaStillRejected)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegatePayloadsTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(DelegatePayloadsTests::MakeModule(
			TEXT("Lambda"),
			TEXT("int Run() { return function(){ return 42; }(); }\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("AnonymousFunction"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		(void)Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		ASSERT_THAT(IsTrue(InModules[0]->bCompileError));
		const FString Combined = Engine->FormatDiagnostics();
		ASSERT_THAT(IsTrue(
			Combined.Contains(TEXT("AnonymousFunction"))
				|| Combined.Contains(TEXT("3008")),
			*Combined));
	}
};

#endif
