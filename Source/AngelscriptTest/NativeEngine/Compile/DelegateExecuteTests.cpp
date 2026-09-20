#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateExecuteTests
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
		Module->ModuleName = FString::Printf(TEXT("DelegateExecute.%s"), Token);
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

TEST_CLASS_WITH_FLAGS(DelegateExecute, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OrdinaryRetValExecute)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateExecuteTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::Compile(*Engine, TEXT("Ordinary"),
			TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FAddOne, int);\n")
			TEXT("int AddOne(int Value) { return Value + 1; }\n")
			TEXT("int Run() { FAddOne Handler; Handler.Bind(AddOne); return Handler.Execute(41); }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateExecuteTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError) == 42, *ExecuteError));
	}

	TEST_METHOD(MulticastBroadcast)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateExecuteTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::Compile(*Engine, TEXT("Multicast"),
			TEXT("DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnValue, int, int&inout, int&inout);\n")
			TEXT("void ListenA(int Value, int&inout First, int&inout Second) { First = Value; Second = Value; }\n")
			TEXT("void ListenB(int Value, int&inout First, int&inout Second) { First = Value; Second = Value; }\n")
			TEXT("int Run() { int First = 0; int Second = 0; FOnValue Ev; Ev.Add(ListenA); Ev.Add(ListenB); Ev.Broadcast(7, First, Second); return First * 100 + Second; }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateExecuteTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegateExecuteTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(707, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(DynamicRetValExecute)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateExecuteTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::Compile(*Engine, TEXT("Dynamic"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int, FAddOne, int, Value);\n")
			TEXT("int AddOne(int Value) { return Value + 1; }\n")
			TEXT("int Run() { FAddOne Handler; Handler.Bind(AddOne); return Handler.Execute(41); }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateExecuteTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError) == 42, *ExecuteError));
	}

	TEST_METHOD(EmptyExecuteFails)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateExecuteTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateExecuteTests::Compile(*Engine, TEXT("Empty"),
			TEXT("DECLARE_DELEGATE_RetVal_OneParam(int, FAddOne, int);\n")
			TEXT("int Run() { FAddOne Handler; return Handler.Execute(1); }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateExecuteTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		asIScriptContext* Context = Engine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(IsTrue(Context->Prepare(Run) >= 0));
		ASSERT_THAT(IsTrue(Context->Execute() == asEXECUTION_EXCEPTION));
	}

	TEST_METHOD(KeywordStillRejected)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateExecuteTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TArray<TSharedRef<FAngelscriptModuleDesc>> InModules;
		InModules.Add(DelegateExecuteTests::MakeModule(TEXT("Keyword"), TEXT("delegate int FAddOne(int);\n")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> OutModules;
		TestRunner->AddExpectedError(TEXT("stage-3-failed"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(
			TEXT("Hot reload failed due to script compile errors. Keeping all old script code."),
			EAutomationExpectedErrorFlags::Contains,
			1);
		(void)Engine->CompileModules(ECompileType::Initial, InModules, OutModules);
		ASSERT_THAT(IsTrue(InModules[0]->bCompileError));
		const FString Combined = Engine->FormatDiagnostics();
		ASSERT_THAT(IsTrue(
			Combined.Contains(TEXT("removed-delegate-event-keyword"))
				|| Combined.Contains(TEXT("3244")),
			*Combined));
	}
};

#endif
