#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"

#include "StartAngelscriptHeaders.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITEntryPlanReflectedLayoutTests,
	"Angelscript.TestModule.StaticJIT.EntryPlan.ReflectedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(WorldContextOverlayRetainsFormalOrderAndChangesIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITSharedEntryPlanReflectedLayout"),
			ASTEST_AS(R"AS(
				int ConsumeReflectedLayout(int8 First, int64 Second)
				{
					return First + int(Second);
				}
			)AS"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			ModuleScope.GetModule().GetFunctionByDecl(
				"int ConsumeReflectedLayout(int8, int64)"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FStaticJITEntryPlan SharedPlan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptStaticJITEntryPlan(
			*Function, true, true, true, SharedPlan, &Error), Error));
		ASSERT_THAT(AreEqual(2, SharedPlan.Parameters.Num()));
		if (SharedPlan.Parameters.Num() != 2)
		{
			return;
		}
		const FAngelscriptHash256 ScriptSignatureHash =
			SharedPlan.EntryPlanHash;

		SharedPlan.ReflectedParmsLayout =
			EAngelscriptStaticJITReflectedParmsLayout::
				AppendGeneratedWorldContext;
		FinalizeAngelscriptStaticJITEntryPlan(SharedPlan);
		ASSERT_THAT(IsTrue(SharedPlan.IsStructurallyComplete()));
		ASSERT_THAT(IsTrue(!(SharedPlan.EntryPlanHash == ScriptSignatureHash),
			TEXT("The generated WorldContext suffix must participate in Entry Plan identity")));
		ASSERT_THAT(AreEqual(0,
			SharedPlan.Parameters[0].FormalParameterIndex));
		ASSERT_THAT(AreEqual(0,
			SharedPlan.Parameters[0].ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(1,
			SharedPlan.Parameters[1].FormalParameterIndex));
		ASSERT_THAT(AreEqual(1,
			SharedPlan.Parameters[1].ReflectedParameterIndex));

		FAngelscriptTypedASTJITProviderEntryPlan ProviderPlan;
		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITProviderEntryPlan(
			SharedPlan, ProviderPlan, &Error), Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITReflectedParmsLayout::
				AppendGeneratedWorldContext,
			ProviderPlan.ReflectedParmsLayout));

		ASSERT_THAT(IsTrue(BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			EAngelscriptTypedASTJITDirectCapability::RecursionBudget,
			ProviderPlan.InstrumentationProfile,
			&Error), Error));
		FAngelscriptTypedASTJITEmission Body;
		Body.bSuccess = true;
		Body.InstrumentationProfileIdentityHash =
			ProviderPlan.InstrumentationProfile.IdentityHash;
		Body.Definition = TEXT(
			"int32 {SYMBOL_PREFIX}_TypedBody(int8 First, int64 Second)\n"
			"{\n"
			"\treturn static_cast<int32>(First + Second);\n"
			"}\n");

		const FAngelscriptTypedASTJITProviderEmission Emission =
			EmitAngelscriptTypedASTJITProviderFunction(ProviderPlan, Body);
		ASSERT_THAT(IsTrue(Emission.bSuccess, Emission.FailureDetail));
		const FString& Source = Emission.ImplementationTemplate;
		const int32 FirstPosition = Source.Find(
			TEXT("const SIZE_T ParmOffset_0 = ParmsOffset;"));
		const int32 SecondPosition = Source.Find(
			TEXT("const SIZE_T ParmOffset_1 = ParmsOffset;"));
		const int32 WorldContextPosition = Source.Find(
			TEXT("const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;"));
		const int32 ReturnPosition = Source.Find(
			TEXT("const SIZE_T ReturnParmOffset = ParmsOffset;"));
		ASSERT_THAT(IsTrue(
			FirstPosition != INDEX_NONE
				&& FirstPosition < SecondPosition
				&& SecondPosition < WorldContextPosition
				&& WorldContextPosition < ReturnPosition,
			TEXT("Reflected offsets must follow formal order, then the hidden WorldContext suffix, then return storage")));
		ASSERT_THAT(IsTrue(Source.Contains(
			TEXT("ParmsOffset = Align(ParmsOffset, alignof(int64));"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
