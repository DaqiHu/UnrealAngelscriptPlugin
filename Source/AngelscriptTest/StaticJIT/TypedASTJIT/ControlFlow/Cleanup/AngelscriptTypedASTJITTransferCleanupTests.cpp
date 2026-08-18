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

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITTransferCleanupTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.ControlFlow.Cleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptTypedASTJITFunctionShape MakeShape(
		const asCScriptFunction& Function,
		const asCTypedSemanticFunction& HIR)
	{
		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = TEXT("ASJIT_TransferCleanup");
		for (asUINT ParameterIndex = 0;
			ParameterIndex < Function.parameterTypes.GetLength();
			++ParameterIndex)
		{
			Shape.ParameterTypes.Add(Function.parameterTypes[ParameterIndex]);
		}
		Shape.ReturnType = Function.returnType;
		Shape.InvocationKind = HIR.header.invocationKind;
		Shape.ReceiverKind = HIR.header.receiver.kind;
		return Shape;
	}

public:
	TEST_METHOD(VerifiedTargetsAndExplicitEmptyCleanupGateStructuredTransfers)
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
			int TransferCleanup(int Limit, int Selector)
			{
				int Total = 0;
				for (int Outer = 0; Outer < Limit; ++Outer)
				{
					{
						int Scoped = Outer;
						switch (Selector)
						{
						case 0:
						{
							int CaseScoped = 1;
							Total += CaseScoped;
							continue;
						}
						case 1:
						{
							int CaseScoped = 2;
							Total += CaseScoped;
							break;
						}
						default:
							break;
						}
						if (Scoped > 1)
						{
							break;
						}
						Total += Scoped;
					}
				}
				return Total;
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITTransferCleanup",
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
		asCScriptFunction* const ScriptFunction = FindFunction(
			*TestRunner, *ScriptModule, "TransferCleanup");
		const asCTypedSemanticFunction* const HIR = FindVerifiedHIR(
			*TestRunner, *ScriptModule, "TransferCleanup");
		ASSERT_THAT(IsNotNull(ScriptFunction));
		ASSERT_THAT(IsNotNull(HIR));
		if (ScriptFunction == nullptr || HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::VerifiedEmpty,
			HIR->header.cleanupPlanState));
		ASSERT_THAT(IsTrue(HIR->header.cleanupPlanCoversAllTransfers));
		ASSERT_THAT(IsTrue(HIR->header.universalCleanupPlan.IsValid()));
		ASSERT_THAT(AreEqual(1u, HIR->cleanupPlans.GetLength()));
		ASSERT_THAT(AreEqual(
			0u,
			HIR->cleanupPlans[
				HIR->header.universalCleanupPlan.value]
				.reverseLiveLifetimeSlots.GetLength(),
			TEXT("Scalar exited scopes must be represented by an explicit empty plan")));

		asTypedSemanticStatementId ContinueId;
		int32 BreakToSwitchCount = 0;
		int32 BreakToLoopCount = 0;
		int32 TransferWithExitedScopeCount = 0;
		for (asUINT Index = 0; Index < HIR->statements.GetLength(); ++Index)
		{
			const asSTypedSemanticStatement& Statement = HIR->statements[Index];
			if (Statement.kind != asETypedSemanticStatementKind::Break
				&& Statement.kind != asETypedSemanticStatementKind::Continue
				&& Statement.kind != asETypedSemanticStatementKind::Return)
			{
				continue;
			}
			ASSERT_THAT(AreEqual(
				HIR->header.universalCleanupPlan,
				Statement.cleanupPlan,
				TEXT("Every transfer must name the compiler-authored universal empty cleanup plan")));
			if (Statement.exitedScopeCount > 0)
			{
				++TransferWithExitedScopeCount;
			}
			if (Statement.kind == asETypedSemanticStatementKind::Continue)
			{
				ContinueId = Statement.id;
				ASSERT_THAT(AreEqual(
					asETypedSemanticStatementKind::For,
					HIR->statements[Statement.targetStatement.value].kind,
					TEXT("Continue nested in switch must target the enclosing for loop")));
			}
			else if (Statement.kind == asETypedSemanticStatementKind::Break)
			{
				const asETypedSemanticStatementKind TargetKind =
					HIR->statements[Statement.targetStatement.value].kind;
				if (TargetKind == asETypedSemanticStatementKind::Switch)
				{
					++BreakToSwitchCount;
				}
				else if (TargetKind == asETypedSemanticStatementKind::For)
				{
					++BreakToLoopCount;
				}
			}
		}
		ASSERT_THAT(IsTrue(ContinueId.IsValid()));
		ASSERT_THAT(IsTrue(BreakToSwitchCount >= 2));
		ASSERT_THAT(IsTrue(BreakToLoopCount >= 1));
		ASSERT_THAT(IsTrue(TransferWithExitedScopeCount >= 1));

		FAngelscriptTypedASTJITEmission ValidEmission;
		ASSERT_THAT(IsTrue(TryEmitFunction(
			*TestRunner,
			*ScriptModule,
			"TransferCleanup",
			TEXT("ASJIT_TransferCleanup"),
			ValidEmission)));
		ASSERT_THAT(IsTrue(ValidEmission.Definition.Contains(
			TEXT("continue;"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(ValidEmission.Definition.Contains(
			TEXT("break;"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsFalse(ValidEmission.Definition.Contains(
			TEXT("goto "), ESearchCase::CaseSensitive)));

		const FAngelscriptTypedASTJITFunctionShape Shape =
			MakeShape(*ScriptFunction, *HIR);
		asCTypedSemanticFunction InvalidTarget = *HIR;
		InvalidTarget.statements[ContinueId.value].targetStatement =
			InvalidTarget.rootStatement;
		const FAngelscriptTypedASTJITEmission InvalidTargetEmission =
			EmitTypedASTJITFunction(InvalidTarget, Shape, {});
		ASSERT_THAT(IsFalse(InvalidTargetEmission.bSuccess));
		ASSERT_THAT(IsTrue(InvalidTargetEmission.Definition.IsEmpty(),
			TEXT("An invalid target must fail before emitting partial C++")));
		ASSERT_THAT(IsTrue(InvalidTargetEmission.FailureDetail.Contains(
			TEXT("Continue must target the nearest enclosing loop"),
			ESearchCase::CaseSensitive)));

		asCTypedSemanticFunction InvalidCleanup = *HIR;
		InvalidCleanup.statements[ContinueId.value].cleanupPlan =
			asTypedSemanticCleanupPlanId();
		const FAngelscriptTypedASTJITEmission InvalidCleanupEmission =
			EmitTypedASTJITFunction(InvalidCleanup, Shape, {});
		ASSERT_THAT(IsFalse(InvalidCleanupEmission.bSuccess));
		ASSERT_THAT(IsTrue(InvalidCleanupEmission.Definition.IsEmpty(),
			TEXT("A missing cleanup plan must fail before emitting partial C++")));
		ASSERT_THAT(IsTrue(InvalidCleanupEmission.FailureDetail.Contains(
			TEXT("Control transfer requires a compiler-authored cleanup plan"),
			ESearchCase::CaseSensitive)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
