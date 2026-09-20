#pragma once

#include "AngelscriptSettings.h"
#include "UObject/Object.h"

class asITypeInfo;
class asIScriptEngine;
struct asSJITFunctionBinding;

class FAngelscriptEngine
{
public:
	FAngelscriptEngine()
		: ConfigSettings(&Settings)
	{
	}

	static FAngelscriptEngine& Get()
	{
		static FAngelscriptEngine Instance;
		return Instance;
	}

	static FAngelscriptEngine* TryGetCurrentEngine()
	{
		return &Get();
	}

	static bool IsSimulatingCookedForCurrentContext()
	{
		return false;
	}

	static bool AllowsJITBindingExecution(
		asIScriptEngine*, const asSJITFunctionBinding&)
	{
		return true;
	}

	static bool CanCastScriptObjectToUnrealInterface(
		asITypeInfo*, asITypeInfo*, void*)
	{
		return false;
	}

	UAngelscriptSettings* ConfigSettings = nullptr;

private:
	UAngelscriptSettings Settings;
};
