#include "StaticJIT/AngelscriptStaticJITBytecodeAccessAudit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

#include <atomic>

namespace AngelscriptStaticJITBytecodeAccessAudit_Private
{
	struct FAtomicObservation
	{
		std::atomic<uint64> TypedASTGenerationScopeCount{0};
		std::atomic<uint64> BytecodeAnalysisCount{0};
		std::atomic<uint64> BytecodeReferenceScanCount{0};
		std::atomic<uint64> GetByteCodeCount{0};
		std::atomic<uint64> BytecodeDispatchCount{0};
		std::atomic<uint64> BytecodeAnalysisDuringTypedASTCount{0};
		std::atomic<uint64> BytecodeReferenceScanDuringTypedASTCount{0};
		std::atomic<uint64> GetByteCodeDuringTypedASTCount{0};
		std::atomic<uint64> BytecodeDispatchDuringTypedASTCount{0};
	};

	FAtomicObservation GObservation;
	thread_local uint32 GTypedASTGenerationDepth = 0;

	void Increment(
		std::atomic<uint64>& Total,
		std::atomic<uint64>& DuringTypedAST)
	{
		Total.fetch_add(1, std::memory_order_relaxed);
		if (GTypedASTGenerationDepth > 0)
		{
			DuringTypedAST.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void Reset(std::atomic<uint64>& Counter)
	{
		Counter.store(0, std::memory_order_relaxed);
	}

	uint64 Read(const std::atomic<uint64>& Counter)
	{
		return Counter.load(std::memory_order_relaxed);
	}
}

void FAngelscriptStaticJITBytecodeAccessAudit::ResetForTesting()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	Reset(GObservation.TypedASTGenerationScopeCount);
	Reset(GObservation.BytecodeAnalysisCount);
	Reset(GObservation.BytecodeReferenceScanCount);
	Reset(GObservation.GetByteCodeCount);
	Reset(GObservation.BytecodeDispatchCount);
	Reset(GObservation.BytecodeAnalysisDuringTypedASTCount);
	Reset(GObservation.BytecodeReferenceScanDuringTypedASTCount);
	Reset(GObservation.GetByteCodeDuringTypedASTCount);
	Reset(GObservation.BytecodeDispatchDuringTypedASTCount);
}

FAngelscriptStaticJITBytecodeAccessObservation
FAngelscriptStaticJITBytecodeAccessAudit::GetObservationForTesting()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	FAngelscriptStaticJITBytecodeAccessObservation Result;
	Result.TypedASTGenerationScopeCount =
		Read(GObservation.TypedASTGenerationScopeCount);
	Result.BytecodeAnalysisCount = Read(GObservation.BytecodeAnalysisCount);
	Result.BytecodeReferenceScanCount =
		Read(GObservation.BytecodeReferenceScanCount);
	Result.GetByteCodeCount = Read(GObservation.GetByteCodeCount);
	Result.BytecodeDispatchCount = Read(GObservation.BytecodeDispatchCount);
	Result.BytecodeAnalysisDuringTypedASTCount =
		Read(GObservation.BytecodeAnalysisDuringTypedASTCount);
	Result.BytecodeReferenceScanDuringTypedASTCount =
		Read(GObservation.BytecodeReferenceScanDuringTypedASTCount);
	Result.GetByteCodeDuringTypedASTCount =
		Read(GObservation.GetByteCodeDuringTypedASTCount);
	Result.BytecodeDispatchDuringTypedASTCount =
		Read(GObservation.BytecodeDispatchDuringTypedASTCount);
	return Result;
}

void FAngelscriptStaticJITBytecodeAccessAudit::BeginTypedASTGeneration()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	++GTypedASTGenerationDepth;
	GObservation.TypedASTGenerationScopeCount.fetch_add(
		1, std::memory_order_relaxed);
}

void FAngelscriptStaticJITBytecodeAccessAudit::EndTypedASTGeneration()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	check(GTypedASTGenerationDepth > 0);
	--GTypedASTGenerationDepth;
}

void FAngelscriptStaticJITBytecodeAccessAudit::RecordBytecodeAnalysis()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	Increment(
		GObservation.BytecodeAnalysisCount,
		GObservation.BytecodeAnalysisDuringTypedASTCount);
}

void FAngelscriptStaticJITBytecodeAccessAudit::RecordBytecodeReferenceScan()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	Increment(
		GObservation.BytecodeReferenceScanCount,
		GObservation.BytecodeReferenceScanDuringTypedASTCount);
}

void FAngelscriptStaticJITBytecodeAccessAudit::RecordGetByteCode()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	Increment(
		GObservation.GetByteCodeCount,
		GObservation.GetByteCodeDuringTypedASTCount);
}

void FAngelscriptStaticJITBytecodeAccessAudit::RecordBytecodeDispatch()
{
	using namespace AngelscriptStaticJITBytecodeAccessAudit_Private;
	Increment(
		GObservation.BytecodeDispatchCount,
		GObservation.BytecodeDispatchDuringTypedASTCount);
}

#endif // WITH_ANGELSCRIPT_UNITTESTS
