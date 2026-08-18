#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITEntryPlanReceiverTests,
	"Angelscript.TestModule.StaticJIT.EntryPlan.Receiver",
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

	TEST_METHOD(NativeObjectSlotDoesNotReplaceFormalParameterZero)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITSharedEntryPlanNativeReceiver"),
			ASTEST_AS(R"AS(
				class FSharedEntryPlanOwner
				{
					int ConsumeValue(int Value)
					{
						return Value;
					}
				}
			)AS"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asITypeInfo* const Type =
			ModuleScope.GetModule().GetTypeInfoByName("FSharedEntryPlanOwner");
		ASSERT_THAT(IsNotNull(Type));
		asCScriptFunction* const Function = Type != nullptr
			? static_cast<asCScriptFunction*>(
				Type->GetMethodByDecl("int ConsumeValue(int)"))
			: nullptr;
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FStaticJITEntryPlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptStaticJITEntryPlan(
			*Function, true, true, true, Plan, &Error), Error));
		ASSERT_THAT(IsTrue(Plan.bHasNativeObjectSlot));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryReceiverKind::NativeObjectSlot,
			Plan.ReceiverKind));
		ASSERT_THAT(AreEqual(INDEX_NONE, Plan.ReceiverParameterIndex));
		ASSERT_THAT(AreEqual(1, Plan.Parameters.Num()));
		if (Plan.Parameters.Num() != 1)
		{
			return;
		}

		const FStaticJITEntryParameterPlan& Value = Plan.Parameters[0];
		ASSERT_THAT(AreEqual(FString(TEXT("Value")), Value.CanonicalName));
		ASSERT_THAT(AreEqual(0, Value.FormalParameterIndex));
		ASSERT_THAT(AreEqual(0, Value.ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryParameterRole::Declared,
			Value.Role));
		ASSERT_THAT(AreEqual(2, Plan.Return.VMResultOffsetDWords));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
