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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITBranchEmissionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Branches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 CountOccurrences(
		const FString& Text,
		const FStringView Needle)
	{
		int32 Count = 0;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 Found = Text.Find(
				Needle.GetData(),
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				SearchFrom);
			if (Found == INDEX_NONE)
			{
				return Count;
			}
			++Count;
			SearchFrom = Found + Needle.Len();
		}
	}

public:
	TEST_METHOD(NestedIfElseUsesVerifiedConditionsAndImmediateFailureEdges)
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
			int NestedBranchTrace(int A, int B, int Divisor)
			{
				int Trace = 0;
				if ((12 / Divisor) > 1)
				{
					if (A > 0)
					{
						Trace = 10;
					}
					else if (B > 0)
					{
						Trace = 20;
					}
					else
					{
						Trace = 30;
					}
				}
				else
				{
					Trace = 40;
				}
				return Trace;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITNestedBranches",
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

		FAngelscriptTypedASTJITEmission Emission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"NestedBranchTrace",
			TEXT("ASJIT_NestedBranchTrace"),
			Emission)));

		const int32 DividePosition = Emission.Definition.Find(
			TEXT("CheckedDivide<int32>(Execution"),
			ESearchCase::CaseSensitive);
		const int32 FailurePosition = Emission.Definition.Find(
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			DividePosition == INDEX_NONE ? 0 : DividePosition + 1);
		const int32 OuterIfPosition = Emission.Definition.Find(
			TEXT("if (as_sem_if_condition_"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			FailurePosition == INDEX_NONE ? 0 : FailurePosition + 1);
		ASSERT_THAT(IsTrue(
			DividePosition != INDEX_NONE
				&& FailurePosition > DividePosition
				&& OuterIfPosition > FailurePosition,
			TEXT("A failed outer condition must return before selecting either branch")));
		ASSERT_THAT(AreEqual(
			3,
			CountOccurrences(
				Emission.Definition,
				TEXTVIEW("const bool as_sem_if_condition_")),
			TEXT("All three source conditions must remain independently materialized")));
		ASSERT_THAT(IsTrue(Emission.Definition.Contains(
			TEXT("else\n"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("goto "), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
