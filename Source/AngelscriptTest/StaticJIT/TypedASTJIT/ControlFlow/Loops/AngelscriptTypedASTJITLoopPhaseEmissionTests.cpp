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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITLoopPhaseEmissionTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Loops.Phases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asTypedSemanticSymbolId FindSymbol(
		const asCTypedSemanticFunction& HIR,
		const char* const Name)
	{
		for (asUINT Index = 0; Index < HIR.symbols.GetLength(); ++Index)
		{
			if (HIR.symbols[Index].name == Name)
			{
				return HIR.symbols[Index].id;
			}
		}
		return asTypedSemanticSymbolId();
	}

public:
	TEST_METHOD(ForWhileAndDoWhileRetainContinueAndIncrementPhases)
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
			int ForContinuePhases(int Limit)
			{
				int Trace = 0;
				for (int Index = 0; Index < Limit; Trace += 100, ++Index)
				{
					Trace += 1;
					if (Index == 0)
					{
						continue;
					}
					Trace += 10;
					break;
				}
				return Trace;
			}

			int WhileContinuePhase(int Limit)
			{
				int Trace = 0;
				int Index = 0;
				while (Index < Limit)
				{
					++Index;
					if (Index == 1)
					{
						continue;
					}
					Trace += 10;
					break;
				}
				return Trace + Index;
			}

			int DoWhileContinuePhase()
			{
				int Trace = 0;
				int Index = 0;
				do
				{
					++Index;
					if (Index == 1)
					{
						continue;
					}
					Trace += 10;
				}
				while (Index < 2);
				return Trace + Index;
			}

			int ForIncrementFailureStopsLaterIncrement(int Divisor)
			{
				int Later = 0;
				for (int Index = 0; Index < 1;
					Index = 10 / Divisor, ++Later)
				{
					++Index;
				}
				return Later;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITLoopPhases",
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

		FAngelscriptTypedASTJITEmission ForEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"ForContinuePhases",
			TEXT("ASJIT_ForContinuePhases"),
			ForEmission)));
		ASSERT_THAT(IsTrue(ForEmission.Definition.Contains(
			TEXT("for (; "), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(ForEmission.Definition.Contains(
			TEXT("continue;"), ESearchCase::CaseSensitive)));

		FAngelscriptTypedASTJITEmission WhileEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"WhileContinuePhase",
			TEXT("ASJIT_WhileContinuePhase"),
			WhileEmission)));
		ASSERT_THAT(IsTrue(WhileEmission.Definition.Contains(
			TEXT("while ("), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(WhileEmission.Definition.Contains(
			TEXT("continue;"), ESearchCase::CaseSensitive)));

		FAngelscriptTypedASTJITEmission DoWhileEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"DoWhileContinuePhase",
			TEXT("ASJIT_DoWhileContinuePhase"),
			DoWhileEmission)));
		ASSERT_THAT(IsTrue(DoWhileEmission.Definition.Contains(
			TEXT("do\n"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(DoWhileEmission.Definition.Contains(
			TEXT("continue;"), ESearchCase::CaseSensitive)));

		const asCTypedSemanticFunction* const FailureHIR =
			FindVerifiedHIR(
				*TestRunner,
				*ScriptModule,
				"ForIncrementFailureStopsLaterIncrement");
		ASSERT_THAT(IsNotNull(FailureHIR));
		if (FailureHIR == nullptr)
		{
			return;
		}
		const asTypedSemanticSymbolId LaterSymbol =
			FindSymbol(*FailureHIR, "Later");
		ASSERT_THAT(IsTrue(LaterSymbol.IsValid()));

		FAngelscriptTypedASTJITEmission FailureEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"ForIncrementFailureStopsLaterIncrement",
			TEXT("ASJIT_ForIncrementFailureStopsLaterIncrement"),
			FailureEmission)));
		const int32 DividePosition = FailureEmission.Definition.Find(
			TEXT("CheckedDivide<int32>(Execution"),
			ESearchCase::CaseSensitive);
		const int32 FailurePosition = FailureEmission.Definition.Find(
			TEXT("if (UNLIKELY(Execution.bExceptionThrown))"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			DividePosition == INDEX_NONE ? 0 : DividePosition + 1);
		const int32 IncrementAbortPosition = FailureEmission.Definition.Find(
			TEXT("return;"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			FailurePosition == INDEX_NONE ? 0 : FailurePosition + 1);
		const FString LaterTarget = FString::Printf(
			TEXT("= as_sem_s%u;"), LaterSymbol.value);
		const int32 LaterIncrementPosition = FailureEmission.Definition.Find(
			LaterTarget,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			IncrementAbortPosition == INDEX_NONE
				? 0 : IncrementAbortPosition + 1);
		ASSERT_THAT(IsTrue(
			DividePosition != INDEX_NONE
				&& FailurePosition > DividePosition
				&& IncrementAbortPosition > FailurePosition
				&& LaterIncrementPosition > IncrementAbortPosition,
			TEXT("A failed first for-increment expression must return from the increment phase before evaluating the later increment")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
