#pragma once

#include "CoreMinimal.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptJITBindingContext.h"

class asCScriptFunction;
class FAutomationTestBase;

namespace AngelscriptStaticJITAotRuntimeRouteTestSupport
{
	struct FEntryExecutionCounts
	{
		uint64 VM = 0;
		uint64 Raw = 0;
		uint64 Parms = 0;

		static FEntryExecutionCounts Capture(
			const FAngelscriptJITBindingContext& BindingContext);

		bool ExpectUnchanged(
			FAutomationTestBase& Test,
			const FAngelscriptJITBindingContext& BindingContext,
			const TCHAR* Label) const;
	};

	FAngelscriptEngine* RequireValidEngine(
		FAutomationTestBase& Test,
		AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession* Session,
		const TCHAR* Scenario);

	asCScriptFunction* FindGlobalFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName& ModuleName,
		const FString& Declaration);

	const FAngelscriptJITBindingContext* RequireBindingContext(
		FAutomationTestBase& Test,
		asCScriptFunction& Function,
		const TCHAR* Scenario);
}
