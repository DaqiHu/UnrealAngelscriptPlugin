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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITReturnEmissionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Returns",
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
	TEST_METHOD(VoidEarlyAndMultipleReturnsEmitWithoutAValueExpression)
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
			void StopAtSelectedBranch(bool bFirst, int Value)
			{
				if (bFirst)
				{
					return;
				}

				if (Value > 3)
				{
					return;
				}
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITVoidReturns",
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
			"StopAtSelectedBranch",
			TEXT("ASJIT_StopAtSelectedBranch"),
			Emission)));
		ASSERT_THAT(AreEqual(
			2,
			CountOccurrences(Emission.Definition, TEXTVIEW("return;")),
			TEXT("Both source early-return statements must remain explicit")));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("return void{}"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(Emission.Definition.Contains(
			TEXT("FAngelscriptJITExecutionContext"),
			ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
