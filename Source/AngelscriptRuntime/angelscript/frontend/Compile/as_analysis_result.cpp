#include "frontend/Compile/as_analysis_result.h"

#include "as_builder.h"
#include "frontend/Compile/as_compilation_session.h"

BEGIN_AS_NAMESPACE

asCAnalysisResult::asCAnalysisResult(
	TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> InInputs,
	TUniquePtr<asCBuilder> InBuilder,
	uint64 InAnalysisID)
	: Inputs(MoveTemp(InInputs))
	, Builder(MoveTemp(InBuilder))
	, AnalysisID(InAnalysisID)
{
}

const asCCompilationSession* asCAnalysisResult::GetSession() const
{
	return Builder.IsValid() ? Builder->GetCompilationSession() : nullptr;
}

asCASTContext* asCAnalysisResult::GetASTContext() const
{
	asCCompilationSession* Session = Builder.IsValid() ? Builder->GetCompilationSession() : nullptr;
	return Session ? &Session->GetASTContext() : nullptr;
}

TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> asCAnalysisResult::GetDiagnosticResult() const
{
	if (Builder.IsValid() && Builder->GetCompileOutput())
	{
		return Builder->GetCompileOutput()->GetDiagnosticResult();
	}
	return MakeShared<asCDiagnosticResult, ESPMode::ThreadSafe>();
}

TConstArrayView<asSDirectiveProcessResult> asCAnalysisResult::GetPreprocessedSources() const
{
	return Builder.IsValid() ? Builder->GetPreprocessedSources() : TConstArrayView<asSDirectiveProcessResult>{};
}

asSAnalysisEntityHandle asCAnalysisResult::MakeHandle(const asCASTNode* Node) const
{
	asSAnalysisEntityHandle Handle;
	if (const asCCompilationSession* Session = GetSession())
	{
		Handle.AnalysisID = AnalysisID;
		Handle.Node = Session->GetASTContext().GetHandle(Node);
	}
	return Handle;
}

const asCASTNode* asCAnalysisResult::ResolveHandle(const asSAnalysisEntityHandle& Handle) const
{
	if (!OwnsHandle(Handle) || !GetSession())
	{
		return nullptr;
	}
	return GetSession()->GetASTContext().Resolve(Handle.Node);
}

bool asCAnalysisResult::OwnsHandle(const asSAnalysisEntityHandle& Handle) const
{
	const asCCompilationSession* Session = GetSession();
	return Handle.AnalysisID == AnalysisID
		&& Session
		&& Handle.Node.IsValid()
		&& Handle.Node.OwnerID == Session->GetASTContext().GetContextID();
}

END_AS_NAMESPACE
