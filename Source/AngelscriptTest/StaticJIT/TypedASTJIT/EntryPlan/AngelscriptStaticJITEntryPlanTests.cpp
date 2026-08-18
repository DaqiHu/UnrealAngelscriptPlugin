#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITEntryPlanTests,
	"Angelscript.TestModule.StaticJIT.EntryPlan.TypedASTJIT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FStaticJITEntryParameterPlan* FindFormalParameter(
		const FStaticJITEntryPlan& Plan,
		const int32 FormalParameterIndex)
	{
		return Plan.Parameters.FindByPredicate(
			[FormalParameterIndex](const FStaticJITEntryParameterPlan& Parameter)
			{
				return Parameter.FormalParameterIndex == FormalParameterIndex;
			});
	}

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

	TEST_METHOD(ScalarPlanFreezesCppVmReflectedAndReturnFacts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITSharedEntryPlanScalar"),
			ASTEST_AS(R"AS(
				int SharedEntryScalar(int Value, uint16 Count)
				{
					return Value + Count;
				}
			)AS"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			ModuleScope.GetModule().GetFunctionByDecl(
				"int SharedEntryScalar(int, uint16)"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FStaticJITEntryPlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptStaticJITEntryPlan(
			*Function, true, true, true, Plan, &Error), Error));
		ASSERT_THAT(IsFalse(Plan.EntryPlanHash.IsZero()));
		ASSERT_THAT(IsTrue(Plan.bHasVMEntry));
		ASSERT_THAT(IsTrue(Plan.bHasRawEntry));
		ASSERT_THAT(IsTrue(Plan.bHasParmsEntry));
		ASSERT_THAT(IsFalse(Plan.bHasNativeObjectSlot));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryReceiverKind::None,
			Plan.ReceiverKind));
		ASSERT_THAT(AreEqual(INDEX_NONE, Plan.ReceiverParameterIndex));
		ASSERT_THAT(AreEqual(2, Plan.Parameters.Num()));

		const FStaticJITEntryParameterPlan* const Value =
			FindFormalParameter(Plan, 0);
		const FStaticJITEntryParameterPlan* const Count =
			FindFormalParameter(Plan, 1);
		ASSERT_THAT(IsNotNull(Value));
		ASSERT_THAT(IsNotNull(Count));
		if (Value == nullptr || Count == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("int32")), Value->BodyCppType));
		ASSERT_THAT(AreEqual(FString(TEXT("asDWORD")), Value->EntryCppType));
		ASSERT_THAT(AreEqual(Function->parameterOffsets[0],
			Value->VMFrameOffsetDWords));
		ASSERT_THAT(AreEqual(0, Value->ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryParameterRole::Declared,
			Value->Role));
		ASSERT_THAT(AreEqual(FString(TEXT("uint16")), Count->BodyCppType));
		ASSERT_THAT(AreEqual(FString(TEXT("asWORD")), Count->EntryCppType));
		ASSERT_THAT(AreEqual(Function->parameterOffsets[1],
			Count->VMFrameOffsetDWords));
		ASSERT_THAT(AreEqual(1, Count->ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(FString(TEXT("int32")),
			Plan.Return.BodyCppType));
		ASSERT_THAT(AreEqual(FString(TEXT("int32")),
			Plan.Return.EntryCppType));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryReturnPlacement::Direct,
			Plan.Return.Placement));
	}

	TEST_METHOD(ExternalImplicitThisKeepsFormalParameterZero)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASStaticJITSharedEntryPlanExternalReceiver"),
			ASTEST_AS(R"AS(
				int SharedEntryExternal(UObject Self, int Value)
					external_implicit_this
				{
					return Value;
				}
			)AS"));
		ASSERT_THAT(IsTrue(ModuleScope.IsValid()));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
			ModuleScope.GetModule().GetFunctionByName("SharedEntryExternal"));
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}

		FStaticJITEntryPlan Plan;
		FString Error;
		ASSERT_THAT(IsTrue(BuildAngelscriptStaticJITEntryPlan(
			*Function, true, true, true, Plan, &Error), Error));
		ASSERT_THAT(IsFalse(Plan.bHasNativeObjectSlot));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryReceiverKind::DeclaredParameterAlias,
			Plan.ReceiverKind));
		ASSERT_THAT(AreEqual(0, Plan.ReceiverParameterIndex));
		ASSERT_THAT(AreEqual(2, Plan.Parameters.Num(),
			TEXT("external_implicit_this must not erase parameter zero")));

		const FStaticJITEntryParameterPlan* const Receiver =
			FindFormalParameter(Plan, 0);
		const FStaticJITEntryParameterPlan* const Value =
			FindFormalParameter(Plan, 1);
		ASSERT_THAT(IsNotNull(Receiver));
		ASSERT_THAT(IsNotNull(Value));
		if (Receiver == nullptr || Value == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, Receiver->ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryParameterRole::ReceiverAlias,
			Receiver->Role));
		ASSERT_THAT(AreEqual(1, Value->FormalParameterIndex));
		ASSERT_THAT(AreEqual(1, Value->ReflectedParameterIndex));
		ASSERT_THAT(AreEqual(
			EAngelscriptStaticJITEntryParameterRole::Declared,
			Value->Role));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
