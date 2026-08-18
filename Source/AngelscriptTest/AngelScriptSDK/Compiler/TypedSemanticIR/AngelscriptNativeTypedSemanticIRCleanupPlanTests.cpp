#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRCleanupPlanTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.CleanupPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCapturedFunction
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		TUniquePtr<AngelscriptNativeTestSupport::FScopedNativeModule> Module;
		asCScriptFunction* Function = nullptr;
		const asCTypedSemanticFunction* HIR = nullptr;
	};

	static const asSTypedSemanticStatement* FindFirstStatement(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticStatementKind Kind)
	{
		for (asUINT Index = 0; Index < HIR.statements.GetLength(); ++Index)
		{
			if (HIR.statements[Index].kind == Kind)
			{
				return &HIR.statements[Index];
			}
		}
		return nullptr;
	}

	static const asSTypedSemanticStatement* FindLocalDeclaration(
		const asCTypedSemanticFunction& HIR,
		const char* const Name)
	{
		for (asUINT Index = 0; Index < HIR.statements.GetLength(); ++Index)
		{
			const asSTypedSemanticStatement& Statement = HIR.statements[Index];
			if (Statement.kind != asETypedSemanticStatementKind::LocalDeclaration
				|| !Statement.symbol.IsValid()
				|| Statement.symbol.value >= HIR.symbols.GetLength())
			{
				continue;
			}
			if (HIR.symbols[Statement.symbol.value].name.Equals(Name))
			{
				return &Statement;
			}
		}
		return nullptr;
	}

	static TArray<const asSTypedSemanticStatement*> FindStatements(
		const asCTypedSemanticFunction& HIR,
		const asETypedSemanticStatementKind Kind)
	{
		TArray<const asSTypedSemanticStatement*> Result;
		for (asUINT Index = 0; Index < HIR.statements.GetLength(); ++Index)
		{
			if (HIR.statements[Index].kind == Kind)
			{
				Result.Add(&HIR.statements[Index]);
			}
		}
		return Result;
	}

	static const asSTypedSemanticCleanupPlan* GetPlan(
		const asCTypedSemanticFunction& HIR,
		const asTypedSemanticCleanupPlanId PlanId)
	{
		if (!PlanId.IsValid() || PlanId.value >= HIR.cleanupPlans.GetLength())
		{
			return nullptr;
		}
		return &HIR.cleanupPlans[PlanId.value];
	}

	static FString SlotList(const asCArray<asUINT>& Slots)
	{
		FString Result = TEXT("[");
		for (asUINT Index = 0; Index < Slots.GetLength(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}
			Result += FString::FromInt(static_cast<int32>(Slots[Index]));
		}
		Result += TEXT("]");
		return Result;
	}

	bool CaptureFunction(
		const char* const ModuleName,
		const char* const Declaration,
		const std::string& ScriptSource,
		FCapturedFunction& OutCaptured)
	{
		using namespace AngelscriptNativeTestSupport;

		OutCaptured.Engine.Create(*TestRunner);
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(OutCaptured.Engine.Get());
		if (!TestRunner->TestNotNull(
				TEXT("Cleanup-plan capture should own a script engine"),
				ScriptEngine))
		{
			return false;
		}
		if (!TestRunner->TestTrue(
				TEXT("Cleanup-plan capture should enable HIR before build"),
				ScriptEngine->SetTypedSemanticIRCapture(true)))
		{
			return false;
		}

		OutCaptured.Module = MakeUnique<FScopedNativeModule>(
			*TestRunner,
			OutCaptured.Engine,
			ModuleName,
			ScriptSource);
		if (!TestRunner->TestTrue(
				*FString::Printf(
					TEXT("%hs should compile with capture on"),
					ModuleName),
				OutCaptured.Module->IsValid()))
		{
			const FString Messages = OutCaptured.Engine.GetMessagesText();
			if (!Messages.IsEmpty())
			{
				TestRunner->AddError(Messages);
			}
			return false;
		}

		OutCaptured.Function = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(*OutCaptured.Module, Declaration));
		if (!TestRunner->TestNotNull(
				*FString::Printf(
					TEXT("%hs should publish %hs"),
					ModuleName,
					Declaration),
				OutCaptured.Function))
		{
			return false;
		}

		OutCaptured.HIR = OutCaptured.Function->GetTypedSemanticFunction();
		const FString Diagnostic = UTF8_TO_TCHAR(
			OutCaptured.Function->GetTypedSemanticCaptureDiagnostic().AddressOf());
		if (!TestRunner->TestNotNull(
				*FString::Printf(
					TEXT("%hs should publish verified HIR; diagnostic=%s"),
					Declaration,
					*Diagnostic),
				OutCaptured.HIR))
		{
			return false;
		}

		const asSTypedSemanticVerificationResult Verification =
			VerifyTypedSemanticFunction(*OutCaptured.HIR);
		if (!TestRunner->TestTrue(
				*FString::Printf(
					TEXT("%hs HIR must verify: %s\n%s"),
					Declaration,
					UTF8_TO_TCHAR(Verification.detail.AddressOf()),
					UTF8_TO_TCHAR(
						DumpTypedSemanticFunction(*OutCaptured.HIR).AddressOf())),
				Verification.IsValid()))
		{
			return false;
		}
		return true;
	}

public:
	TEST_METHOD(TwoManagedLocalsPublishReversePerEdgePlans)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CLEANUP-PLAN-REVERSE",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FManagedLifetime
			{
				int Value;
			}

			int TwoManagedLocals(int Early)
			{
				FManagedLifetime First = FManagedLifetime();
				if (Early != 0)
				{
					return 1;
				}
				FManagedLifetime Second = FManagedLifetime();
				return 2;
			}
			)AS");

		FCapturedFunction Captured;
		ON_SCOPE_EXIT
		{
			Captured.Module.Reset();
			Captured.Engine.Destroy();
		};
		if (!CaptureFunction(
				"TypedSemanticIRCleanupPlanReverse",
				"int TwoManagedLocals(int)",
				ScriptSource,
				Captured))
		{
			return;
		}

		const asCTypedSemanticFunction& HIR = *Captured.HIR;
		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::PartialConstruction,
			HIR.header.cleanupPlanState,
			*FString::Printf(
				TEXT("A later managed constructor after an earlier live local is a partial-construction plan; state=%d dump=%s"),
				static_cast<int32>(HIR.header.cleanupPlanState),
				UTF8_TO_TCHAR(DumpTypedSemanticFunction(HIR).AddressOf()))));
		ASSERT_THAT(IsFalse(
			HIR.header.cleanupPlanCoversAllTransfers,
			TEXT("Different live sets cannot share one universal cleanup plan")));
		ASSERT_THAT(IsTrue(HIR.header.hasExceptionCleanup));
		ASSERT_THAT(IsTrue(HIR.cleanupPlans.GetLength() >= 2));

		const asSTypedSemanticStatement* const FirstDecl =
			FindLocalDeclaration(HIR, "First");
		const asSTypedSemanticStatement* const SecondDecl =
			FindLocalDeclaration(HIR, "Second");
		ASSERT_THAT(IsNotNull(FirstDecl));
		ASSERT_THAT(IsNotNull(SecondDecl));
		if (FirstDecl == nullptr || SecondDecl == nullptr)
		{
			return;
		}

		const TArray<const asSTypedSemanticStatement*> Returns =
			FindStatements(HIR, asETypedSemanticStatementKind::Return);
		ASSERT_THAT(AreEqual(2, Returns.Num(),
			TEXT("The fixture must expose one early return and one late return")));
		if (Returns.Num() != 2)
		{
			return;
		}

		const asSTypedSemanticCleanupPlan* const EarlyPlan =
			GetPlan(HIR, Returns[0]->cleanupPlan);
		const asSTypedSemanticCleanupPlan* const LatePlan =
			GetPlan(HIR, Returns[1]->cleanupPlan);
		ASSERT_THAT(IsNotNull(EarlyPlan,
			TEXT("The early return must name the live-set cleanup plan")));
		ASSERT_THAT(IsNotNull(LatePlan,
			TEXT("The late return must name the live-set cleanup plan")));
		if (EarlyPlan == nullptr || LatePlan == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1u, EarlyPlan->reverseLiveLifetimeSlots.GetLength(),
			TEXT("Only First is live on the early return")));
		ASSERT_THAT(AreEqual(0u, EarlyPlan->reverseLiveLifetimeSlots[0],
			TEXT("The sole live slot on the early return is the first declaration")));
		ASSERT_THAT(AreEqual(2u, LatePlan->reverseLiveLifetimeSlots.GetLength(),
			*FString::Printf(
				TEXT("Both locals are live on the late return; slots=%s"),
				*SlotList(LatePlan->reverseLiveLifetimeSlots))));
		ASSERT_THAT(AreEqual(1u, LatePlan->reverseLiveLifetimeSlots[0],
			TEXT("Later declarations must be destroyed first")));
		ASSERT_THAT(AreEqual(0u, LatePlan->reverseLiveLifetimeSlots[1],
			TEXT("Earlier declarations must be destroyed last")));
		ASSERT_THAT(AreNotEqual(
			EarlyPlan->id,
			LatePlan->id,
			TEXT("Distinct live sets must be distinct cleanup plans")));
	}

	TEST_METHOD(PartialConstructionFailureExcludesUnconstructedSlot)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CLEANUP-PLAN-PARTIAL",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FManagedLifetime
			{
				int Value;
			}

			class FFailingLifetime
			{
				int Value;

				FFailingLifetime()
				{
					int Zero = 0;
					Value = 1 / Zero;
				}
			}

			int PartialConstruction()
			{
				FManagedLifetime First = FManagedLifetime();
				FFailingLifetime Second = FFailingLifetime();
				return First.Value;
			}
			)AS");

		FCapturedFunction Captured;
		ON_SCOPE_EXIT
		{
			Captured.Module.Reset();
			Captured.Engine.Destroy();
		};
		if (!CaptureFunction(
				"TypedSemanticIRCleanupPlanPartial",
				"int PartialConstruction()",
				ScriptSource,
				Captured))
		{
			return;
		}

		const asCTypedSemanticFunction& HIR = *Captured.HIR;
		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::PartialConstruction,
			HIR.header.cleanupPlanState,
			TEXT("A later constructor that can fail must publish a partial-construction plan")));
		ASSERT_THAT(IsFalse(HIR.header.cleanupPlanCoversAllTransfers));

		const asSTypedSemanticStatement* const SecondDecl =
			FindLocalDeclaration(HIR, "Second");
		ASSERT_THAT(IsNotNull(SecondDecl,
			TEXT("The failing constructor must remain a local declaration")));
		if (SecondDecl == nullptr)
		{
			return;
		}

		const asSTypedSemanticCleanupPlan* const FailurePlan =
			GetPlan(HIR, SecondDecl->cleanupPlan);
		ASSERT_THAT(IsNotNull(FailurePlan,
			TEXT("The failing local must own the initializer-failure cleanup plan")));
		if (FailurePlan == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1u, FailurePlan->reverseLiveLifetimeSlots.GetLength(),
			TEXT("Initializer failure must destroy only earlier live slots")));
		ASSERT_THAT(AreEqual(0u, FailurePlan->reverseLiveLifetimeSlots[0],
			TEXT("The unconstructed Second slot must be excluded from failure cleanup")));
	}

	TEST_METHOD(ReturnOnStackOmitsReturnedSlotFromCleanup)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CLEANUP-PLAN-RETURN-ON-STACK",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FCleanupHandle
			{
				int Value;
			}

			struct FCleanupValue
			{
				int First;
				int Second;
				int Third;
				int Fourth;
			}

			FCleanupValue ReturnOwnedValue()
			{
				FCleanupHandle Leftover = FCleanupHandle();
				FCleanupValue Result;
				Result.First = 7;
				return Result;
			}
			)AS");

		FCapturedFunction Captured;
		ON_SCOPE_EXIT
		{
			Captured.Module.Reset();
			Captured.Engine.Destroy();
		};
		if (!CaptureFunction(
				"TypedSemanticIRCleanupPlanReturnOnStack",
				"FCleanupValue ReturnOwnedValue()",
				ScriptSource,
				Captured))
		{
			return;
		}

		const asCTypedSemanticFunction& HIR = *Captured.HIR;
		ASSERT_THAT(IsTrue(
			HIR.header.returnsOnStack,
			TEXT("The returned script value must keep return-on-stack ownership")));
		ASSERT_THAT(IsTrue(
			HIR.header.cleanupPlanState
				== asETypedSemanticCleanupPlanState::NonEmpty
				|| HIR.header.cleanupPlanState
					== asETypedSemanticCleanupPlanState::PartialConstruction
				|| HIR.header.cleanupPlanState
					== asETypedSemanticCleanupPlanState::ScriptDestructor,
			*FString::Printf(
				TEXT("Return-on-stack with a leftover local still needs a cleanup plan; state=%d"),
				static_cast<int32>(HIR.header.cleanupPlanState))));

		const asSTypedSemanticStatement* const Return =
			FindFirstStatement(HIR, asETypedSemanticStatementKind::Return);
		ASSERT_THAT(IsNotNull(Return));
		if (Return == nullptr)
		{
			return;
		}

		const asSTypedSemanticCleanupPlan* const Plan =
			GetPlan(HIR, Return->cleanupPlan);
		ASSERT_THAT(IsNotNull(Plan,
			TEXT("The return must name a cleanup plan that preserves the returned slot")));
		if (Plan == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1u, Plan->reverseLiveLifetimeSlots.GetLength(),
			*FString::Printf(
				TEXT("Only Leftover is callee-owned at return; slots=%s"),
				*SlotList(Plan->reverseLiveLifetimeSlots))));
		ASSERT_THAT(AreEqual(0u, Plan->reverseLiveLifetimeSlots[0],
			TEXT("Leftover is the first declared managed local and remains callee-owned")));
	}

	TEST_METHOD(ScriptDestructorValuesPublishScriptDestructorState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CLEANUP-PLAN-SCRIPT-DESTRUCTOR",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FTrackedValue
			{
				int Value;

				~FTrackedValue()
				{
				}
			}

			int TwoScriptDestructors()
			{
				FTrackedValue First = FTrackedValue();
				FTrackedValue Second = FTrackedValue();
				return First.Value + Second.Value;
			}
			)AS");

		FCapturedFunction Captured;
		ON_SCOPE_EXIT
		{
			Captured.Module.Reset();
			Captured.Engine.Destroy();
		};
		if (!CaptureFunction(
				"TypedSemanticIRCleanupPlanScriptDestructor",
				"int TwoScriptDestructors()",
				ScriptSource,
				Captured))
		{
			return;
		}

		const asCTypedSemanticFunction& HIR = *Captured.HIR;
		ASSERT_THAT(AreEqual(
			asETypedSemanticCleanupPlanState::ScriptDestructor,
			HIR.header.cleanupPlanState,
			*FString::Printf(
				TEXT("User script destructors must be an explicit cleanup state; state=%d dump=%s"),
				static_cast<int32>(HIR.header.cleanupPlanState),
				UTF8_TO_TCHAR(DumpTypedSemanticFunction(HIR).AddressOf()))));
		ASSERT_THAT(IsTrue(HIR.header.hasExceptionCleanup));

		const asSTypedSemanticStatement* const Return =
			FindFirstStatement(HIR, asETypedSemanticStatementKind::Return);
		ASSERT_THAT(IsNotNull(Return));
		if (Return == nullptr)
		{
			return;
		}
		const asSTypedSemanticCleanupPlan* const Plan =
			GetPlan(HIR, Return->cleanupPlan);
		ASSERT_THAT(IsNotNull(Plan));
		if (Plan == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(2u, Plan->reverseLiveLifetimeSlots.GetLength()));
		ASSERT_THAT(AreEqual(1u, Plan->reverseLiveLifetimeSlots[0],
			TEXT("Second must be destroyed before First")));
		ASSERT_THAT(AreEqual(0u, Plan->reverseLiveLifetimeSlots[1]));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
