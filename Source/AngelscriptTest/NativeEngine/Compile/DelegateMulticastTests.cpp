#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace DelegateMulticastTests
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
		Module->ModuleName = FString::Printf(TEXT("DelegateMulticast.%s"), Token);
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

TEST_CLASS_WITH_FLAGS(DelegateMulticast, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RemoveLeavesPeer)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateMulticastTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateMulticastTests::Compile(*Engine, TEXT("Remove"),
			TEXT("DECLARE_MULTICAST_DELEGATE_ThreeParams(FOn, int, int&inout, int&inout);\n")
			TEXT("void ListenA(int Value, int&inout First, int&inout Second) { First = Value; }\n")
			TEXT("void ListenB(int Value, int&inout First, int&inout Second) { Second = Value; }\n")
			TEXT("int Run() { int First = 0; int Second = 0; FOn Ev; uint64 Handle = Ev.Add(ListenA); Ev.Add(ListenB); Ev.Remove(Handle); Ev.Broadcast(42, First, Second); return First * 100 + Second; }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateMulticastTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegateMulticastTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(42, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(DuplicateAdd)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateMulticastTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateMulticastTests::Compile(*Engine, TEXT("Duplicate"),
			TEXT("DECLARE_MULTICAST_DELEGATE_ThreeParams(FOn, int, int&inout, int&inout);\n")
			TEXT("void Listen(int Value, int&inout First, int&inout Second) { First = Second; Second = Value; }\n")
			TEXT("int Run() { int First = 0; int Second = 0; FOn Ev; Ev.Add(Listen); Ev.Add(Listen); Ev.Broadcast(1, First, Second); return First * 100 + Second; }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateMulticastTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegateMulticastTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(101, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(CopiedDomain)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateMulticastTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateMulticastTests::Compile(*Engine, TEXT("Copied"),
			TEXT("DECLARE_MULTICAST_DELEGATE_ThreeParams(FOn, int, int&inout, int&inout);\n")
			TEXT("void ListenA(int Value, int&inout First, int&inout Second) { First = Value; }\n")
			TEXT("void ListenB(int Value, int&inout First, int&inout Second) { Second = Value; }\n")
			TEXT("int Run() { int First = 0; int Second = 0; FOn Ev; uint64 Handle = Ev.Add(ListenA); Ev.Add(ListenB); FOn Copy = Ev; Copy.Remove(Handle); Copy.Broadcast(42, First, Second); return First * 100 + Second; }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateMulticastTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegateMulticastTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(4242, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}

	TEST_METHOD(AddMissesCurrentRound)
	{
		FString Diagnostic;
		TUniquePtr<FAngelscriptEngine> Engine = DelegateMulticastTests::MakeHostEngine(Diagnostic);
		ASSERT_THAT(IsNotNull(Engine.Get(), *Diagnostic));
		FAngelscriptEngineScope Scope(*Engine);

		TSharedPtr<FAngelscriptModuleDesc> Module;
		FString Error;
		ASSERT_THAT(IsTrue(DelegateMulticastTests::Compile(*Engine, TEXT("MissRound"),
			TEXT("DECLARE_MULTICAST_DELEGATE_TwoParams(FOn, int, int&inout);\n")
			TEXT("void Late(int Value, int&inout Hits) { Hits = Value; }\n")
			TEXT("void Early(int Value, int&inout Hits, FOn Ev) { Ev.Add(Late); }\n")
			TEXT("int Run() { int Hits = 0; FOn Ev; uint64 Seed = Ev.Add(Late); Ev.Remove(Seed); Ev.Add(Early, Ev); Ev.Broadcast(42, Hits); return Hits; }\n"),
			Module, Error), *Error));
		asIScriptFunction* Run = DelegateMulticastTests::FindGlobal(Engine->GetScriptEngine(), "Run");
		ASSERT_THAT(IsNotNull(Run, TEXT("Run")));
		FString ExecuteError;
		const int32 Got = DelegateMulticastTests::ExecuteInt(Engine->GetScriptEngine(), Run, ExecuteError);
		ASSERT_THAT(AreEqual(0, Got, *FString::Printf(TEXT("got=%d %s"), Got, *ExecuteError)));
	}
};

#endif
