/* Maintained Unreal AngelScript fork — owned tooling analysis snapshot. */
#pragma once

#include "frontend/Sema/as_tooling_types.h"

BEGIN_AS_NAMESPACE

class asCBuilder;
class asCCompilationSession;

class ANGELSCRIPTRUNTIME_API asCAnalysisResult
{
public:
	asCAnalysisResult(
		TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> InInputs,
		TUniquePtr<asCBuilder> InBuilder,
		uint64 InAnalysisID);

	uint64 GetAnalysisID() const { return AnalysisID; }
	const asSAnalysisInputs& GetInputs() const { return Inputs.Get(); }
	const asCCompilationSession* GetSession() const;
	asCASTContext* GetASTContext() const;
	TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> GetDiagnosticResult() const;
	TConstArrayView<asSDirectiveProcessResult> GetPreprocessedSources() const;
	asSAnalysisEntityHandle MakeHandle(const asCASTNode* Node) const;
	const asCASTNode* ResolveHandle(const asSAnalysisEntityHandle& Handle) const;
	bool OwnsHandle(const asSAnalysisEntityHandle& Handle) const;

private:
	TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> Inputs;
	TUniquePtr<asCBuilder> Builder;
	uint64 AnalysisID = 0;
};

END_AS_NAMESPACE
