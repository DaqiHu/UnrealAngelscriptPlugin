#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "ClassGenerator/ASFunction.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AOT/RuntimeRoutes/AngelscriptStaticJITAotRuntimeRouteTestSupport.h"

#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	using namespace AngelscriptStaticJITAotRuntimeRouteTestSupport;
	using EExecutionCapability = EAngelscriptJITExecutionCapability;

	enum class EDebuggerRequirementScenario : uint8
	{
		ConnectedDebugger,
		BreakNext,
		DataBreakpointLocals,
	};

	const TCHAR* GetDebuggerRequirementScenarioName(
		const EDebuggerRequirementScenario Scenario)
	{
		switch (Scenario)
		{
		case EDebuggerRequirementScenario::ConnectedDebugger:
			return TEXT("connected debugger");
		case EDebuggerRequirementScenario::BreakNext:
			return TEXT("break-next");
		case EDebuggerRequirementScenario::DataBreakpointLocals:
			return TEXT("data-breakpoint locals");
		}
		return TEXT("unknown debugger requirement");
	}

	EExecutionCapability GetExpectedDebuggerRequirements(
		const EDebuggerRequirementScenario Scenario)
	{
		EExecutionCapability Required = EExecutionCapability::FramePosition;
		switch (Scenario)
		{
		case EDebuggerRequirementScenario::ConnectedDebugger:
			return Required
				| EExecutionCapability::LineCallback
				| EExecutionCapability::DebuggerStep
				| EExecutionCapability::DebuggerLocals;
		case EDebuggerRequirementScenario::BreakNext:
			return Required
				| EExecutionCapability::LineCallback
				| EExecutionCapability::DebuggerStep;
		case EDebuggerRequirementScenario::DataBreakpointLocals:
			return Required
				| EExecutionCapability::LineCallback
				| EExecutionCapability::DebuggerLocals;
		}
		return Required;
	}

	bool RunDebuggerActiveRoutes(
		FAutomationTestBase& Test,
		AngelscriptStaticJITAotGeneration::
			FStaticJITAotFixtureEngineSession* const Session,
		const EDebuggerRequirementScenario Scenario)
	{
#if !WITH_AS_DEBUGSERVER
		Test.AddError(TEXT(
			"TypedASTJIT debugger routing proof requires WITH_AS_DEBUGSERVER."));
		return false;
#else
		FAngelscriptEngine* const EnginePtr = RequireValidEngine(
			Test, Session, TEXT("TypedASTJIT debugger routing"));
		if (EnginePtr == nullptr)
		{
			return false;
		}
		FAngelscriptEngine& Engine = *EnginePtr;
		FAngelscriptEngineScope EngineScope(Engine);
		if (!Test.TestNotNull(
				TEXT("TypedASTJIT runtime fixture should own its real DebugServer"),
				Engine.DebugServer))
		{
			return false;
		}

		UClass* const StaticsClass = FindGeneratedClass(
			&Engine,
			AngelscriptStaticJITAotFixture::
				GetTypedControlFlowStaticsClassName());
		UASFunction* const StructuredWrapper = StaticsClass != nullptr
			? Cast<UASFunction>(FindGeneratedFunction(
				StaticsClass, TEXT("SemanticStructuredControlFlow")))
			: nullptr;
		asCScriptFunction* const StructuredFunction =
			StructuredWrapper != nullptr
				? static_cast<asCScriptFunction*>(
					StructuredWrapper->ScriptFunction)
				: nullptr;
		FIntProperty* const LimitProperty = StructuredWrapper != nullptr
			? FindFProperty<FIntProperty>(StructuredWrapper, TEXT("Limit"))
			: nullptr;
		FIntProperty* const SelectorProperty = StructuredWrapper != nullptr
			? FindFProperty<FIntProperty>(StructuredWrapper, TEXT("Selector"))
			: nullptr;
		FIntProperty* const StructuredReturnProperty =
			StructuredWrapper != nullptr
				? FindFProperty<FIntProperty>(
					StructuredWrapper, TEXT("ReturnValue"))
				: nullptr;
		FObjectPropertyBase* const WorldContextProperty =
			StructuredWrapper != nullptr
				? FindFProperty<FObjectPropertyBase>(
					StructuredWrapper, TEXT("_World_Context"))
				: nullptr;

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

		if (!Test.TestNotNull(
				TEXT("Debugger route should resolve the structured statics class"),
				StaticsClass)
			|| !Test.TestNotNull(
				TEXT("Debugger route should resolve the structured UASFunction"),
				StructuredWrapper)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain the structured script function"),
				StructuredFunction)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain Limit"), LimitProperty)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain Selector"), SelectorProperty)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain the structured return value"),
				StructuredReturnProperty)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain WorldContext"),
				WorldContextProperty)
			|| !Test.TestNotNull(
				TEXT("Debugger route should resolve the final-scalar class"),
				FinalScalarClass)
			|| !Test.TestNotNull(
				TEXT("Debugger route should resolve the Raw UASFunction"),
				FinalScalarWrapper)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain the Raw script function"),
				FinalScalarFunction)
			|| !Test.TestNotNull(
				TEXT("Debugger route should retain the Raw return property"),
				FinalScalarReturnProperty))
		{
			return false;
		}

		const asSJITFunctionBinding StructuredBinding =
			StructuredFunction->GetJITBinding();
		const asSJITFunctionBinding FinalScalarBinding =
			FinalScalarFunction->GetJITBinding();
		const FAngelscriptJITBindingContext* const StructuredContext =
			RequireBindingContext(
				Test, *StructuredFunction, TEXT("Structured debugger route"));
		const FAngelscriptJITBindingContext* const FinalScalarContext =
			RequireBindingContext(
				Test, *FinalScalarFunction, TEXT("Raw debugger route"));
		if (StructuredContext == nullptr || FinalScalarContext == nullptr
			|| !Test.TestNotNull(
				TEXT("Structured debugger route should expose a Parms entry"),
				reinterpret_cast<const void*>(StructuredBinding.ParmsEntry))
			|| !Test.TestNotNull(
				TEXT("Final-scalar debugger route should expose a Raw entry"),
				reinterpret_cast<const void*>(FinalScalarBinding.RawEntry)))
		{
			return false;
		}

		const FEntryExecutionCounts StructuredCounts =
			FEntryExecutionCounts::Capture(*StructuredContext);
		const FEntryExecutionCounts FinalScalarCounts =
			FEntryExecutionCounts::Capture(*FinalScalarContext);

		const bool bWasDebugging = Engine.DebugServer->bIsDebugging;
		const bool bWasBreakNext =
			Engine.DebugServer->bBreakNextScriptLine.Load();
		const TArray<FAngelscriptDataBreakpoint> PreviousDataBreakpoints =
			Engine.DebugServer->DataBreakpoints;
		ON_SCOPE_EXIT
		{
			Engine.DebugServer->bIsDebugging = bWasDebugging;
			Engine.DebugServer->bBreakNextScriptLine.Store(bWasBreakNext);
			Engine.DebugServer->DataBreakpoints = PreviousDataBreakpoints;
			Engine.UpdateLineCallbackState();
		};

		Engine.DebugServer->bIsDebugging = false;
		Engine.DebugServer->bBreakNextScriptLine.Store(false);
		Engine.DebugServer->DataBreakpoints.Reset();
		switch (Scenario)
		{
		case EDebuggerRequirementScenario::ConnectedDebugger:
			Engine.DebugServer->bIsDebugging = true;
			break;
		case EDebuggerRequirementScenario::BreakNext:
			Engine.DebugServer->bBreakNextScriptLine.Store(true);
			break;
		case EDebuggerRequirementScenario::DataBreakpointLocals:
		{
			static int32 DataBreakpointProbeValue = 0;
			FAngelscriptDataBreakpoint DataBreakpoint {};
			DataBreakpoint.Id = 1;
			DataBreakpoint.Address = reinterpret_cast<uint64>(
				&DataBreakpointProbeValue);
			DataBreakpoint.AddressSize = sizeof(DataBreakpointProbeValue);
			DataBreakpoint.Name = TEXT("TypedASTJITRouteProbe");
			Engine.DebugServer->DataBreakpoints.Add(MoveTemp(DataBreakpoint));
			break;
		}
		}
		Engine.UpdateLineCallbackState();

		const EExecutionCapability CurrentRequirements =
			Engine.CaptureCurrentJITExecutionRequirements();
		if (!Test.TestEqual(
				*FString::Printf(
					TEXT("%s should publish the exact execution requirements"),
					GetDebuggerRequirementScenarioName(Scenario)),
				static_cast<uint32>(CurrentRequirements),
				static_cast<uint32>(
					GetExpectedDebuggerRequirements(Scenario))))
		{
			return false;
		}

		int32 ContextResult = 0;
		{
			asIScriptContext* const Context = Engine.CreateContext();
			if (!Test.TestNotNull(
					TEXT("Debugger-active VM route should create an AS context"),
					Context))
			{
				return false;
			}
			ON_SCOPE_EXIT { Context->Release(); };
			if (!Test.TestEqual(
					TEXT("Debugger-active VM route should prepare the Typed function"),
					Context->Prepare(StructuredFunction), asSUCCESS)
				|| !Test.TestEqual(
					TEXT("Debugger-active VM route should bind Limit"),
					Context->SetArgDWord(0, 4), asSUCCESS)
				|| !Test.TestEqual(
					TEXT("Debugger-active VM route should bind Selector"),
					Context->SetArgDWord(1, 2), asSUCCESS)
				|| !Test.TestEqual(
					TEXT("Debugger-active VM route should finish in the interpreter"),
					Context->Execute(), asEXECUTION_FINISHED))
			{
				return false;
			}
			ContextResult = static_cast<int32>(Context->GetReturnDWord());
		}

		TStrongObjectPtr<UObject> FinalScalarInstance(NewObject<UObject>(
			GetTransientPackage(), FinalScalarClass));
		FStructOnScope FinalScalarParams(FinalScalarWrapper);
		FinalScalarWrapper->RuntimeCallEvent(
			FinalScalarInstance.Get(), FinalScalarParams.GetStructMemory());

		FStructOnScope StructuredParams(StructuredWrapper);
		LimitProperty->SetPropertyValue_InContainer(
			StructuredParams.GetStructMemory(), 4);
		SelectorProperty->SetPropertyValue_InContainer(
			StructuredParams.GetStructMemory(), 2);
		UObject* const StaticsObject = StaticsClass->GetDefaultObject();
		WorldContextProperty->SetObjectPropertyValue_InContainer(
			StructuredParams.GetStructMemory(), StaticsObject);
		StructuredWrapper->RuntimeCallEvent(
			StaticsObject, StructuredParams.GetStructMemory());

		return Test.TestEqual(
				TEXT("Debugger-active VM fallback should preserve the script result"),
				ContextResult, 84)
			&& Test.TestEqual(
				TEXT("Debugger-active Raw fallback should preserve the script result"),
				FinalScalarReturnProperty->GetPropertyValue_InContainer(
					FinalScalarParams.GetStructMemory()),
				84)
			&& Test.TestEqual(
				TEXT("Debugger-active Parms fallback should preserve the script result"),
				StructuredReturnProperty->GetPropertyValue_InContainer(
					StructuredParams.GetStructMemory()),
				84)
			&& StructuredCounts.ExpectUnchanged(
				Test, *StructuredContext,
				TEXT("Debugger-active context and Parms fallbacks"))
			&& FinalScalarCounts.ExpectUnchanged(
				Test, *FinalScalarContext,
				TEXT("Debugger-active Raw fallback"));
#endif
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotDebuggerRouteTests,
	"Angelscript.TestModule.StaticJIT.AOT.RuntimeRoutes.Debugger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static TUniquePtr<AngelscriptStaticJITAotGeneration::
		FStaticJITAotFixtureEngineSession> SharedFixtureSession;

public:
	BEFORE_ALL()
	{
		SharedFixtureSession =
			AngelscriptStaticJITAotGeneration::
				CreateFixtureEngineSession(false);
	}

	AFTER_ALL()
	{
		SharedFixtureSession.Reset();
	}

	TEST_METHOD(ActiveRequirementsRouteVmRawAndParmsToInterpreter)
	{
		ASSERT_THAT(IsTrue(RunDebuggerActiveRoutes(
			*TestRunner, SharedFixtureSession.Get(),
			EDebuggerRequirementScenario::ConnectedDebugger)));
	}

	TEST_METHOD(BreakNextRoutesVmRawAndParmsToInterpreter)
	{
		ASSERT_THAT(IsTrue(RunDebuggerActiveRoutes(
			*TestRunner, SharedFixtureSession.Get(),
			EDebuggerRequirementScenario::BreakNext)));
	}

	TEST_METHOD(DataBreakpointLocalsRouteVmRawAndParmsToInterpreter)
	{
		ASSERT_THAT(IsTrue(RunDebuggerActiveRoutes(
			*TestRunner, SharedFixtureSession.Get(),
			EDebuggerRequirementScenario::DataBreakpointLocals)));
	}
};

#endif
