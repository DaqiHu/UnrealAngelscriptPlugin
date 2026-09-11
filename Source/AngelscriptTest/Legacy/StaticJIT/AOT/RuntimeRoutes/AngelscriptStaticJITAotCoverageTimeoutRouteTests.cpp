#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "ClassGenerator/ASFunction.h"
#include "Core/AngelscriptSettings.h"
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AOT/RuntimeRoutes/AngelscriptStaticJITAotRuntimeRouteTestSupport.h"
#include "Testing/AngelscriptTestSettings.h"

#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	using namespace AngelscriptStaticJITAotRuntimeRouteTestSupport;

	bool RunCoverageActiveRoute(FAutomationTestBase& Test)
	{
#if !WITH_AS_COVERAGE
		Test.AddError(TEXT(
			"TypedASTJIT coverage routing proof requires WITH_AS_COVERAGE."));
		return false;
#else
		UAngelscriptTestSettings* const Settings =
			GetMutableDefault<UAngelscriptTestSettings>();
		const bool bWasCoverageEnabled = Settings->bEnableCodeCoverage;
		Settings->bEnableCodeCoverage = true;
		ON_SCOPE_EXIT
		{
			Settings->bEnableCodeCoverage = bWasCoverageEnabled;
		};

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
			AngelscriptStaticJITAotGeneration::
				CreateFixtureEngineSession(false);
		FAngelscriptEngine* const EnginePtr = RequireValidEngine(
			Test, Session.Get(), TEXT("TypedASTJIT coverage routing"));
		if (EnginePtr == nullptr)
		{
			return false;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);

		FAngelscriptCodeCoverage* const Coverage =
			FAngelscriptCodeCoverageExtension::GetForEngine(Engine);
		TSharedPtr<FAngelscriptModuleDesc> Module = Engine.GetModule(
			AngelscriptStaticJITAotFixture::
				GetTypedControlFlowModuleName().ToString());
		asCScriptFunction* const ScriptFunction = FindGlobalFunction(
			Test,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
			AngelscriptStaticJITAotFixture::
				GetSemanticStructuredControlFlowDeclaration());
		if (!Test.TestNotNull(
				TEXT("Coverage route should attach a real per-Engine recorder"),
				Coverage)
			|| !Test.TestTrue(
				TEXT("Coverage route should resolve its module descriptor"),
				Module.IsValid())
			|| ScriptFunction == nullptr)
		{
			return false;
		}

		const FAngelscriptJITBindingContext* const BindingContext =
			RequireBindingContext(
				Test, *ScriptFunction, TEXT("Coverage-active route"));
		if (BindingContext == nullptr)
		{
			return false;
		}
		const FEntryExecutionCounts CountsBefore =
			FEntryExecutionCounts::Capture(*BindingContext);

		Coverage->MapExecutableLines(*Module);
		Coverage->StartRecording();
		ON_SCOPE_EXIT { Coverage->StopRecording(); };

		const FLineCoverage* const BeforeCoverage =
			Coverage->GetLineCoverage(*Module);
		if (!Test.TestNotNull(
				TEXT("Coverage route should map executable lines"),
				BeforeCoverage)
			|| !Test.TestEqual(
				TEXT("Coverage route should begin with no line hits"),
				BeforeCoverage->NumLinesHit(), 0))
		{
			return false;
		}

		asIScriptContext* const Context = Engine.CreateContext();
		if (!Test.TestNotNull(
				TEXT("Coverage route should create its real AS context"),
				Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		if (!Test.TestEqual(
				TEXT("Coverage route should prepare the installed function"),
				Context->Prepare(ScriptFunction), asSUCCESS)
			|| !Test.TestEqual(
				TEXT("Coverage route should bind Limit"),
				Context->SetArgDWord(0, 4), asSUCCESS)
			|| !Test.TestEqual(
				TEXT("Coverage route should bind Selector"),
				Context->SetArgDWord(1, 2), asSUCCESS)
			|| !Test.TestEqual(
				TEXT("Coverage route should finish through the interpreter"),
				Context->Execute(), asEXECUTION_FINISHED))
		{
			return false;
		}

		const FLineCoverage* const AfterCoverage =
			Coverage->GetLineCoverage(*Module);
		return Test.TestEqual(
				TEXT("Coverage-active VM fallback should preserve the script result"),
				static_cast<int32>(Context->GetReturnDWord()), 84)
			&& Test.TestNotNull(
				TEXT("Coverage-active VM fallback should retain mapped coverage"),
				AfterCoverage)
			&& Test.TestTrue(
				TEXT("Coverage-active VM fallback should record real script line hits"),
				AfterCoverage != nullptr && AfterCoverage->NumLinesHit() > 0)
			&& CountsBefore.ExpectUnchanged(
				Test, *BindingContext, TEXT("Coverage-active VM fallback"));
#endif
	}

	bool RunLoopTimeoutRouting(FAutomationTestBase& Test)
	{
		UAngelscriptSettings& Settings = UAngelscriptSettings::Get();
		const float PreviousTimeout =
			Settings.EditorMaximumScriptExecutionTime;
		Settings.EditorMaximumScriptExecutionTime = 0.0f;
		ON_SCOPE_EXIT
		{
			Settings.EditorMaximumScriptExecutionTime = PreviousTimeout;
		};

		TUniquePtr<AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession> Session =
			AngelscriptStaticJITAotGeneration::
				CreateFixtureEngineSession(false);
		FAngelscriptEngine* const EnginePtr = RequireValidEngine(
			Test, Session.Get(), TEXT("TypedASTJIT loop-timeout routing"));
		if (EnginePtr == nullptr)
		{
			return false;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);

		asCScriptFunction* const LoopFunction = FindGlobalFunction(
			Test,
			Engine,
			AngelscriptStaticJITAotFixture::GetTypedControlFlowModuleName(),
			AngelscriptStaticJITAotFixture::
				GetSemanticStructuredControlFlowDeclaration());
		UClass* const FinalScalarClass = FindGeneratedClass(
			&Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedFinalScalarReceiverClassName());
		UASFunction* const FinalScalarWrapper = FinalScalarClass != nullptr
			? Cast<UASFunction>(FindGeneratedFunction(
				FinalScalarClass, TEXT("TypedFinalRawValue")))
			: nullptr;
		asCScriptFunction* const FinalScalarFunction =
			FinalScalarWrapper != nullptr
				? static_cast<asCScriptFunction*>(
					FinalScalarWrapper->ScriptFunction)
				: nullptr;
		FIntProperty* const FinalScalarReturnProperty =
			FinalScalarWrapper != nullptr
				? CastField<FIntProperty>(
					FinalScalarWrapper->GetReturnProperty())
				: nullptr;
		if (LoopFunction == nullptr
			|| !Test.TestNotNull(
				TEXT("Loop-timeout route should resolve the no-loop final class"),
				FinalScalarClass)
			|| !Test.TestNotNull(
				TEXT("Loop-timeout route should resolve the no-loop Raw wrapper"),
				FinalScalarWrapper)
			|| !Test.TestNotNull(
				TEXT("Loop-timeout route should retain the no-loop script function"),
				FinalScalarFunction)
			|| !Test.TestNotNull(
				TEXT("Loop-timeout route should retain the no-loop return property"),
				FinalScalarReturnProperty))
		{
			return false;
		}

		const FAngelscriptJITBindingContext* const LoopBindingContext =
			RequireBindingContext(
				Test, *LoopFunction, TEXT("Loop-timeout structured route"));
		const FAngelscriptJITBindingContext* const NoLoopBindingContext =
			RequireBindingContext(
				Test, *FinalScalarFunction, TEXT("Loop-timeout no-loop route"));
		if (LoopBindingContext == nullptr || NoLoopBindingContext == nullptr)
		{
			return false;
		}
		if (!Test.TestTrue(
				TEXT("Loop route should use a Typed capability-profile entry"),
				LoopBindingContext->HasExecutionCapabilityProfile())
			|| !Test.TestTrue(
				TEXT("No-loop route should use a Typed capability-profile entry"),
				NoLoopBindingContext->HasExecutionCapabilityProfile()))
		{
			return false;
		}

		const FEntryExecutionCounts LoopCountsBeforeBaseline =
			FEntryExecutionCounts::Capture(*LoopBindingContext);
		const FEntryExecutionCounts NoLoopCounts =
			FEntryExecutionCounts::Capture(*NoLoopBindingContext);

		auto ExecuteLoop = [&Test, &Engine, LoopFunction](
			const TCHAR* const Scenario,
			int32& OutResult)
		{
			asIScriptContext* const Context = Engine.CreateContext();
			if (!Test.TestNotNull(
					*FString::Printf(
						TEXT("%s should create a configured AS context"), Scenario),
					Context))
			{
				return false;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (!Test.TestEqual(
					*FString::Printf(
						TEXT("%s should prepare the loop-bearing function"), Scenario),
					Context->Prepare(LoopFunction), asSUCCESS)
				|| !Test.TestEqual(
					*FString::Printf(TEXT("%s should bind Limit"), Scenario),
					Context->SetArgDWord(0, 4), asSUCCESS)
				|| !Test.TestEqual(
					*FString::Printf(TEXT("%s should bind Selector"), Scenario),
					Context->SetArgDWord(1, 2), asSUCCESS)
				|| !Test.TestEqual(
					*FString::Printf(TEXT("%s should finish"), Scenario),
					Context->Execute(), asEXECUTION_FINISHED))
			{
				return false;
			}
			OutResult = static_cast<int32>(Context->GetReturnDWord());
			return true;
		};

		int32 BaselineResult = 0;
		if (!ExecuteLoop(TEXT("Timeout-disabled Typed baseline"), BaselineResult)
			|| !Test.TestEqual(
				TEXT("Timeout-disabled Typed baseline should preserve the result"),
				BaselineResult, 84)
			|| !Test.TestEqual(
				TEXT("Timeout-disabled loop should enter Typed VMEntry once"),
				LoopBindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::VM),
				LoopCountsBeforeBaseline.VM + 1))
		{
			return false;
		}
		const FEntryExecutionCounts LoopCountsBeforeTimeout =
			FEntryExecutionCounts::Capture(*LoopBindingContext);

		// Change the requirement after the Provider is already installed. The
		// invocation gate must re-evaluate it instead of relying on install time.
		Settings.EditorMaximumScriptExecutionTime = 60.0f;
		int32 TimeoutResult = 0;
		if (!ExecuteLoop(TEXT("Timeout-enabled VM fallback"), TimeoutResult))
		{
			return false;
		}

		TStrongObjectPtr<UObject> FinalScalarInstance(NewObject<UObject>(
			GetTransientPackage(), FinalScalarClass));
		FStructOnScope FinalScalarParams(FinalScalarWrapper);
		FinalScalarWrapper->RuntimeCallEvent(
			FinalScalarInstance.Get(), FinalScalarParams.GetStructMemory());

		return Test.TestEqual(
				TEXT("Loop-timeout VM fallback should preserve the loop result"),
				TimeoutResult, 84)
			&& LoopCountsBeforeTimeout.ExpectUnchanged(
				Test, *LoopBindingContext,
				TEXT("Loop-bearing timeout fallback"))
			&& Test.TestEqual(
				TEXT("A global timeout must not reject a no-loop Typed Raw entry"),
				FinalScalarReturnProperty->GetPropertyValue_InContainer(
					FinalScalarParams.GetStructMemory()),
				84)
			&& Test.TestEqual(
				TEXT("The no-loop function should enter its Typed Raw body once"),
				NoLoopBindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Raw),
				NoLoopCounts.Raw + 1)
			&& Test.TestEqual(
				TEXT("The no-loop function should not enter Typed VMEntry"),
				NoLoopBindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::VM),
				NoLoopCounts.VM)
			&& Test.TestEqual(
				TEXT("The no-loop function should not enter Typed ParmsEntry"),
				NoLoopBindingContext->GetExecutionCount(
					FAngelscriptJITBindingContext::EEntryKind::Parms),
				NoLoopCounts.Parms);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotCoverageTimeoutRouteTests,
	"Angelscript.TestModule.StaticJIT.AOT.RuntimeRoutes.CoverageTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CoverageActiveRoutesPositionOnlyTypedEntryToVmAndRecordsLines)
	{
		ASSERT_THAT(IsTrue(RunCoverageActiveRoute(*TestRunner)));
	}

	TEST_METHOD(GlobalTimeoutRoutesOnlyLoopBearingTypedEntryToVm)
	{
		ASSERT_THAT(IsTrue(RunLoopTimeoutRouting(*TestRunner)));
	}
};

#endif
