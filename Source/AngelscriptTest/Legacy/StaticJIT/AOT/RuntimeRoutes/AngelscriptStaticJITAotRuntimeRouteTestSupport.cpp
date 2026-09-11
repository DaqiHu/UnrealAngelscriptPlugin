#include "StaticJIT/AOT/RuntimeRoutes/AngelscriptStaticJITAotRuntimeRouteTestSupport.h"

#include "Misc/AutomationTest.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITAotRuntimeRouteTestSupport
{
	FEntryExecutionCounts FEntryExecutionCounts::Capture(
		const FAngelscriptJITBindingContext& BindingContext)
	{
		FEntryExecutionCounts Result;
		Result.VM = BindingContext.GetExecutionCount(
			FAngelscriptJITBindingContext::EEntryKind::VM);
		Result.Raw = BindingContext.GetExecutionCount(
			FAngelscriptJITBindingContext::EEntryKind::Raw);
		Result.Parms = BindingContext.GetExecutionCount(
			FAngelscriptJITBindingContext::EEntryKind::Parms);
		return Result;
	}

	bool FEntryExecutionCounts::ExpectUnchanged(
		FAutomationTestBase& Test,
		const FAngelscriptJITBindingContext& BindingContext,
		const TCHAR* Label) const
	{
		return Test.TestEqual(
			*FString::Printf(
				TEXT("%s must not enter the Typed VMEntry"), Label),
			BindingContext.GetExecutionCount(
				FAngelscriptJITBindingContext::EEntryKind::VM),
			VM)
			&& Test.TestEqual(
				*FString::Printf(
					TEXT("%s must not enter the Typed RawEntry"), Label),
				BindingContext.GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Raw),
				Raw)
			&& Test.TestEqual(
				*FString::Printf(
					TEXT("%s must not enter the Typed ParmsEntry"), Label),
				BindingContext.GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Parms),
				Parms);
	}

	FAngelscriptEngine* RequireValidEngine(
		FAutomationTestBase& Test,
		AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession* const Session,
		const TCHAR* const Scenario)
	{
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s should create an isolated AOT fixture session"),
					Scenario),
				Session))
		{
			return nullptr;
		}
		if (!Test.TestTrue(
				*FString::Printf(
					TEXT("%s should compile and publish current Provider routes"),
					Scenario),
				Session->IsValid()))
		{
			Test.AddError(Session->Error);
			return nullptr;
		}
		return Session->Engine.Get();
	}

	asCScriptFunction* FindGlobalFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName& ModuleName,
		const FString& Declaration)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(ModuleName.ToString());
		if (!Test.TestTrue(
				*FString::Printf(
					TEXT("Runtime-route fixture should resolve module '%s'"),
					*ModuleName.ToString()),
				Module.IsValid() && Module->ScriptModule != nullptr))
		{
			return nullptr;
		}

		FTCHARToUTF8 FunctionDeclaration(*Declaration);
		asIScriptFunction* const Function =
			Module->ScriptModule->GetFunctionByDecl(FunctionDeclaration.Get());
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("Runtime-route fixture should resolve function '%s' in module '%s'"),
					*Declaration,
					*ModuleName.ToString()),
				Function))
		{
			return nullptr;
		}
		return static_cast<asCScriptFunction*>(Function);
	}

	const FAngelscriptJITBindingContext* RequireBindingContext(
		FAutomationTestBase& Test,
		asCScriptFunction& Function,
		const TCHAR* const Scenario)
	{
		const asSJITFunctionBinding Binding = Function.GetJITBinding();
		const FAngelscriptJITBindingContext* const BindingContext =
			FAngelscriptJITBindingContext::FromBindingUserData(
				Binding.UserData);
		Test.TestNotNull(
			*FString::Printf(
				TEXT("%s should retain its Typed AOT binding context"),
				Scenario),
			BindingContext);
		return BindingContext;
	}
}
