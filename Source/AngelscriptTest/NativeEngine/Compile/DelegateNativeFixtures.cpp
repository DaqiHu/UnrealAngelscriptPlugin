#include "NativeEngine/Compile/DelegateNativeFixtures.h"

#include "AngelscriptBinds.h"
#include "Binds/Bind_Delegates.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

TUniquePtr<FAngelscriptEngine> DelegateNativeFixtures::MakeHostEngine(FString& OutDiagnostic)
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

TSharedRef<FAngelscriptModuleDesc> DelegateNativeFixtures::MakeModule(const TCHAR* Token, const FString& Code)
{
	TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
	Module->ModuleName = FString::Printf(TEXT("DelegateNativeInterop.%s"), Token);
	FAngelscriptModuleDesc::FCodeSection Section;
	Section.AbsoluteFilename = Module->ModuleName + TEXT(".as");
	Section.RelativeFilename = Section.AbsoluteFilename;
	Section.Code = Code;
	Module->Code.Add(MoveTemp(Section));
	return Module;
}

asIScriptFunction* DelegateNativeFixtures::FindGlobal(asIScriptEngine* Engine, const char* Name)
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

bool DelegateNativeFixtures::Compile(
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

bool DelegateNativeFixtures::BindIntTransform(
	TDelegate<int32(int32)>& Dest,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	int32 Payload,
	FString& OutError)
{
	return FAngelscriptDelegateOperations::BindScriptIntTransform(Dest, Engine, Function, Payload, OutError);
}

bool DelegateNativeFixtures::AddIntListener(
	TMulticastDelegate<void(int32)>& Dest,
	asIScriptEngine* Engine,
	asIScriptFunction* Function,
	FDelegateHandle& OutHandle,
	FString& OutError,
	int32* ObservedValue)
{
	return FAngelscriptMulticastDelegateOperations::AddScriptIntListener(
		Dest, Engine, Function, OutHandle, OutError, ObservedValue);
}
