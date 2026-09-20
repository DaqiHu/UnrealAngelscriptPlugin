#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"

class asIScriptEngine;
class asIScriptFunction;

struct FDelegateNativeOwner
{
	TMulticastDelegate<void(int32)> Event;
	int32 Hits = 0;
};

namespace DelegateNativeFixtures
{
	TUniquePtr<FAngelscriptEngine> MakeHostEngine(FString& OutDiagnostic);
	TSharedRef<FAngelscriptModuleDesc> MakeModule(const TCHAR* Token, const FString& Code);
	asIScriptFunction* FindGlobal(asIScriptEngine* Engine, const char* Name);
	bool Compile(
		FAngelscriptEngine& Engine,
		const TCHAR* Token,
		const FString& Code,
		TSharedPtr<FAngelscriptModuleDesc>& OutModule,
		FString& OutError);
	bool BindIntTransform(
		TDelegate<int32(int32)>& Dest,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		int32 Payload,
		FString& OutError);
	bool AddIntListener(
		TMulticastDelegate<void(int32)>& Dest,
		asIScriptEngine* Engine,
		asIScriptFunction* Function,
		FDelegateHandle& OutHandle,
		FString& OutError,
		int32* ObservedValue);
}
