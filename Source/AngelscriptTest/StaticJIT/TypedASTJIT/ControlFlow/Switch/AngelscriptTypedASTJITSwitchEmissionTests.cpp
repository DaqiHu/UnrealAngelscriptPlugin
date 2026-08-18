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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITSwitchEmissionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Switch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NestedSwitchDispositionAndExhaustiveEnumEdgeRemainExplicit)
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
			enum EFlowMode
			{
				Zero,
				One,
				Two,
			}

			int NestedSwitchFlow(int Limit, int Selector)
			{
				int Trace = 0;
				for (int Outer = 0; Outer < Limit; ++Outer)
				{
					switch (Selector)
					{
					case 0:
						Trace += 1;
						continue;
					case 1:
						Trace += 10;
						break;
					default:
					{
						int Scoped = 2;
						while (Scoped > 0)
						{
							--Scoped;
							if (Scoped == 1)
							{
								continue;
							}
							break;
						}
						Trace += Scoped;
						break;
					}
					}
					if (Outer > 1)
					{
						break;
					}
				}

				switch (Selector)
				{
				case 0:
					Trace += 100;
					fallthrough;
				case 1:
				{
					int CaseLocal = 20;
					Trace += CaseLocal;
					break;
				}
				default:
					Trace += 300;
					break;
				}
				return Trace;
			}

			int ExhaustiveSwitchFlow(EFlowMode Mode)
			{
				switch (Mode)
				{
				case EFlowMode::Zero:
					return 10;
				case EFlowMode::One:
					return 20;
				case EFlowMode::Two:
					return 30;
				}
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITSwitchFlow",
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

		FAngelscriptTypedASTJITEmission NestedEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"NestedSwitchFlow",
			TEXT("ASJIT_NestedSwitchFlow"),
			NestedEmission)));
		ASSERT_THAT(IsTrue(NestedEmission.Definition.Contains(
			TEXT("continue;"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(NestedEmission.Definition.Contains(
			TEXT("while ("), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(NestedEmission.Definition.Contains(
			TEXT("goto "), ESearchCase::CaseSensitive)));

		const int32 SecondSwitch = NestedEmission.Definition.Find(
			TEXT("switch ("),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			NestedEmission.Definition.Find(
				TEXT("switch ("), ESearchCase::CaseSensitive) + 1);
		const int32 FallthroughCase = NestedEmission.Definition.Find(
			TEXT("case int32(0):"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			SecondSwitch == INDEX_NONE ? 0 : SecondSwitch);
		const int32 NextCase = NestedEmission.Definition.Find(
			TEXT("case int32(1):"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			FallthroughCase == INDEX_NONE ? 0 : FallthroughCase + 1);
		ASSERT_THAT(IsTrue(
			SecondSwitch != INDEX_NONE
				&& FallthroughCase > SecondSwitch
				&& NextCase > FallthroughCase));
		if (FallthroughCase != INDEX_NONE && NextCase > FallthroughCase)
		{
			const FString FallthroughSegment =
				NestedEmission.Definition.Mid(
					FallthroughCase,
					NextCase - FallthroughCase);
			ASSERT_THAT(IsFalse(FallthroughSegment.Contains(
				TEXT("break;"), ESearchCase::CaseSensitive),
				TEXT("An explicit AngelScript fallthrough case must not gain an implicit C++ break")));
		}

		FAngelscriptTypedASTJITEmission ExhaustiveEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"ExhaustiveSwitchFlow",
			TEXT("ASJIT_ExhaustiveSwitchFlow"),
			ExhaustiveEmission)));
		ASSERT_THAT(IsTrue(ExhaustiveEmission.Definition.Contains(
			TEXT("default:"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(ExhaustiveEmission.Definition.Contains(
			TEXT("SetSwitchValueInvalidException(Execution)"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(ExhaustiveEmission.Definition.Contains(
			TEXT("return int32{};"), ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
