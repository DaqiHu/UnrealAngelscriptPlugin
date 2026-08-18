#include "CQTest.h"

#include "../AngelscriptTypedASTJITControlFlowTestSupport.h"
#include "../../../../AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITLoopExceptionEmissionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Loops.Exceptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void AssertOrdered(
		FAutomationTestBase& Test,
		const FString& Text,
		const TCHAR* const First,
		const TCHAR* const Second,
		const TCHAR* const Third,
		const TCHAR* const Message)
	{
		const int32 FirstPosition = Text.Find(
			First, ESearchCase::CaseSensitive);
		const int32 SecondPosition = Text.Find(
			Second,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			FirstPosition == INDEX_NONE ? 0 : FirstPosition + 1);
		const int32 ThirdPosition = Text.Find(
			Third,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			FirstPosition == INDEX_NONE ? 0 : FirstPosition + 1);
		Test.TestTrue(
			Message,
			FirstPosition != INDEX_NONE
				&& SecondPosition > FirstPosition
				&& ThirdPosition > SecondPosition);
	}

public:
	TEST_METHOD(FailingLoopOperandsStopBeforeLaterEffectsAndStores)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptTypedASTJITControlFlowTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ConditionStopsBeforeRightEffect(int Divisor, int Counter)
			{
				int LocalCounter = Counter;
				while ((10 / Divisor) > ++LocalCounter)
				{
					return LocalCounter;
				}
				return LocalCounter;
			}

			int IncrementStopsBeforeTargetStore(int Divisor)
			{
				int Counter = 5;
				for (int Iteration = 0; Iteration < 1; Counter = 10 / Divisor)
				{
					++Iteration;
				}
				return Counter;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITLoopExceptions",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		if (!Module.IsValid())
		{
			return;
		}

		asCModule* const ScriptModule =
			static_cast<asCModule*>(Module.Get());
		ASSERT_THAT(IsNotNull(ScriptModule));
		if (ScriptModule == nullptr)
		{
			return;
		}

		FAngelscriptTypedASTJITEmission ConditionEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"ConditionStopsBeforeRightEffect",
			TEXT("ASJIT_ConditionStopsBeforeRightEffect"),
			ConditionEmission)));
		AssertOrdered(
			*TestRunner,
			ConditionEmission.Definition,
			TEXT("CheckedDivide<int32>(Execution"),
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			TEXT("_right ="),
			TEXT("A failed loop-condition left operand must stop before the later right-side increment"));

		FAngelscriptTypedASTJITEmission IncrementEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"IncrementStopsBeforeTargetStore",
			TEXT("ASJIT_IncrementStopsBeforeTargetStore"),
			IncrementEmission)));
		AssertOrdered(
			*TestRunner,
			IncrementEmission.Definition,
			TEXT("CheckedDivide<int32>(Execution"),
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			TEXT("& as_sem_e"),
			TEXT("A failed for-increment RHS must stop before the mutation target is evaluated or stored"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
