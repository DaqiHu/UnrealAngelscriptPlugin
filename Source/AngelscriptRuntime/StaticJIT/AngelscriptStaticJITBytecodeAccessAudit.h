#pragma once

#include "CoreMinimal.h"

#if WITH_ANGELSCRIPT_UNITTESTS

/**
 * Process-wide test observation for the bytecode-only StaticJIT boundaries.
 *
 * The total counters self-prove that a paired BytecodeJIT fixture actually
 * exercises each sentinel. The DuringTypedAST counters are incremented only
 * while the current thread is inside a production TypedASTJIT Generate call.
 */
struct ANGELSCRIPTRUNTIME_API
FAngelscriptStaticJITBytecodeAccessObservation
{
	uint64 TypedASTGenerationScopeCount = 0;
	uint64 BytecodeAnalysisCount = 0;
	uint64 BytecodeReferenceScanCount = 0;
	uint64 GetByteCodeCount = 0;
	uint64 BytecodeDispatchCount = 0;
	uint64 BytecodeAnalysisDuringTypedASTCount = 0;
	uint64 BytecodeReferenceScanDuringTypedASTCount = 0;
	uint64 GetByteCodeDuringTypedASTCount = 0;
	uint64 BytecodeDispatchDuringTypedASTCount = 0;
};

/** Unit-test-only instrumentation; no symbol or update exists in Shipping. */
class ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITBytecodeAccessAudit
{
public:
	static void ResetForTesting();
	static FAngelscriptStaticJITBytecodeAccessObservation
		GetObservationForTesting();

	static void BeginTypedASTGeneration();
	static void EndTypedASTGeneration();
	static void RecordBytecodeAnalysis();
	static void RecordBytecodeReferenceScan();
	static void RecordGetByteCode();
	static void RecordBytecodeDispatch();
};

/** Covers the complete synchronous production TypedASTJIT Generate call. */
class FScopedAngelscriptTypedASTJITBytecodeIsolationAudit
{
public:
	FScopedAngelscriptTypedASTJITBytecodeIsolationAudit()
	{
		FAngelscriptStaticJITBytecodeAccessAudit::BeginTypedASTGeneration();
	}

	~FScopedAngelscriptTypedASTJITBytecodeIsolationAudit()
	{
		FAngelscriptStaticJITBytecodeAccessAudit::EndTypedASTGeneration();
	}

	FScopedAngelscriptTypedASTJITBytecodeIsolationAudit(
		const FScopedAngelscriptTypedASTJITBytecodeIsolationAudit&) = delete;
	FScopedAngelscriptTypedASTJITBytecodeIsolationAudit& operator=(
		const FScopedAngelscriptTypedASTJITBytecodeIsolationAudit&) = delete;
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
