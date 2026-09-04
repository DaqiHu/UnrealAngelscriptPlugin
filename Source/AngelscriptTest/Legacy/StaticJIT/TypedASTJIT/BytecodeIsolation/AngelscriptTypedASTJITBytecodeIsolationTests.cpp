#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "StaticJIT/AngelscriptStaticJITBytecodeAccessAudit.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITBytecodeIsolationTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.BytecodeIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CompleteTypedGenerationNeverTraversesBytecode)
	{
		using namespace AngelscriptStaticJITAotGeneration;

		FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		ON_SCOPE_EXIT
		{
			FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting();
			FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		};

		const FStaticJITAotGenerationResult Result =
			RunBytecodeIsolationProbe();
		const FAngelscriptStaticJITBytecodeAccessObservation Observation =
			FAngelscriptStaticJITBytecodeAccessAudit::
				GetObservationForTesting();
		const FAngelscriptTypedASTJITTaskObservation TypedObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();

		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Error));
		ASSERT_THAT(IsFalse(
			Result.IsolatedTypedASTDifferentialSymbolPrefix.IsEmpty(),
			TEXT("The fixture must emit a real eligible TypedASTJIT body")));
		ASSERT_THAT(IsTrue(
			TypedObservation.AcceptedTaskCount > 0,
			TEXT("The probe must enter the production TypedASTJIT backend")));
		ASSERT_THAT(IsTrue(
			Observation.TypedASTGenerationScopeCount > 0,
			TEXT("The audit must cover every production TypedASTJIT Generate call")));

		ASSERT_THAT(IsTrue(
			Observation.BytecodeAnalysisCount > 0,
			TEXT("The paired BytecodeJIT probe must self-prove the analysis sentinel")));
		ASSERT_THAT(IsTrue(
			Observation.BytecodeReferenceScanCount > 0,
			TEXT("The paired BytecodeJIT probe must self-prove the reference-scan sentinel")));
		ASSERT_THAT(IsTrue(
			Observation.GetByteCodeCount > 0,
			TEXT("The paired BytecodeJIT probe must self-prove the GetByteCode sentinel")));
		ASSERT_THAT(IsTrue(
			Observation.BytecodeDispatchCount > 0,
			TEXT("The paired BytecodeJIT probe must self-prove the opcode-dispatch sentinel")));

		ASSERT_THAT(AreEqual(
			uint64{0}, Observation.BytecodeAnalysisDuringTypedASTCount,
			TEXT("Typed eligibility and planning must not invoke BytecodeJIT analysis")));
		ASSERT_THAT(AreEqual(
			uint64{0}, Observation.BytecodeReferenceScanDuringTypedASTCount,
			TEXT("Typed reference collection must not scan bytecode")));
		ASSERT_THAT(AreEqual(
			uint64{0}, Observation.GetByteCodeDuringTypedASTCount,
			TEXT("Typed generation must not read the VM instruction stream")));
		ASSERT_THAT(AreEqual(
			uint64{0}, Observation.BytecodeDispatchDuringTypedASTCount,
			TEXT("Typed emission must not dispatch FAngelscriptBytecode")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
