#include "frontend/Sema/as_tooling_session.h"

#include "as_builder.h"
#include "frontend/Compile/as_compilation_session.h"

BEGIN_AS_NAMESPACE

asCToolingSession::asCToolingSession(TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> InInputs)
	: Inputs(MoveTemp(InInputs))
{
}

#if WITH_ANGELSCRIPT_TESTS
void asCToolingSession::DebugPlantForeignRangeOnNextAnalyze()
{
	bDebugPlantForeignRange = true;
}

void asCToolingSession::DebugFailNextDeclarationStage()
{
	bDebugFailDeclarations = true;
}

void asCToolingSession::DebugCancelAt(asEToolingCancelCheckpoint Checkpoint, bool* Flag)
{
	DebugCheckpoint = Checkpoint;
	DebugCancelFlag = Flag;
}

bool asCToolingSession::ConsumeCancelCheckpoint(asEToolingCancelCheckpoint Checkpoint) const
{
	if (DebugCheckpoint != Checkpoint || !DebugCancelFlag)
	{
		return false;
	}
	*DebugCancelFlag = true;
	DebugCheckpoint = asEToolingCancelCheckpoint::None;
	DebugCancelFlag = nullptr;
	return true;
}
#endif

bool asCToolingSession::ValidateInputs() const
{
	if (Inputs->Sources.IsEmpty() || Inputs->Options.StableScope.IsEmpty())
	{
		return false;
	}
	for (asCModuleDefinitionSet* Dependency : Inputs->Options.Dependencies)
	{
		if (!Dependency)
		{
			return false;
		}
		bool bOwned = false;
		for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& Owned : Inputs->OwnedDefinitionSets)
		{
			if (Owned.Get() == Dependency)
			{
				bOwned = true;
				break;
			}
		}
		if (!bOwned)
		{
			return false;
		}
	}
	for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& Owned : Inputs->OwnedDefinitionSets)
	{
		if (!Owned.IsValid() || !Owned->IsFrozen())
		{
			return false;
		}
		for (asCModuleDefinitionSet* Nested : Owned->GetDependencies())
		{
			if (!Nested)
			{
				return false;
			}
			bool bOwnedNested = false;
			for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& Candidate : Inputs->OwnedDefinitionSets)
			{
				if (Candidate.Get() == Nested)
				{
					bOwnedNested = true;
					break;
				}
			}
			if (!bOwnedNested)
			{
				return false;
			}
		}
	}
	return true;
}

asSAnalysisResponse asCToolingSession::Analyze(const asSCancellationToken& Cancel)
{
	asSAnalysisResponse Response;
	if (Cancel.IsCancelled())
	{
		Response.Status = asEAnalysisStatus::Cancelled;
		Response.Coverage = TEXT("pre-cancelled");
		return Response;
	}
	if (!ValidateInputs())
	{
		Response.Status = asEAnalysisStatus::InvalidInput;
		return Response;
	}

	asSBuilderOptions Options = Inputs->Options;
	Options.Dependencies.Reset();
	for (const TSharedPtr<asCModuleDefinitionSet, ESPMode::ThreadSafe>& Owned : Inputs->OwnedDefinitionSets)
	{
		Options.Dependencies.Add(Owned.Get());
	}

	TUniquePtr<asCBuilder> Builder = MakeUnique<asCBuilder>(
		MakeArrayView(Inputs->Sources),
		Options);
	if (Cancel.IsCancelled())
	{
		Response.Status = asEAnalysisStatus::Cancelled;
		Response.Coverage = TEXT("before-bodies");
		return Response;
	}
	const bool bBodies = Builder->RunThrough(asEBuilderStage::BodiesAnalyzed);
	if (Cancel.IsCancelled())
	{
		Response.Status = asEAnalysisStatus::Cancelled;
		Response.Coverage = TEXT("after-bodies");
		return Response;
	}
	asCCompilationSession* Session = Builder->GetCompilationSession();
#if WITH_ANGELSCRIPT_TESTS
	if (bDebugPlantForeignRange && Session)
	{
		bDebugPlantForeignRange = false;
		asCASTContext& Context = Session->GetASTContext();
		for (asCASTNode* Node : Context.GetNodes())
		{
			if (Node && Node->GetFamily() != asEASTNodeFamily::Type && Node->GetSourceRange().IsOrdered())
			{
				asCSourceRange Foreign = Node->GetSourceRange();
				Foreign.Begin.SnapshotID ^= 0x9F17ULL;
				Foreign.End.SnapshotID = Foreign.Begin.SnapshotID;
				Node->DebugForceSourceRange(Foreign);
				break;
			}
		}
	}
#endif
	if (!Session || !Session->GetASTContext().FreezeForTooling())
	{
		Response.Status = asEAnalysisStatus::AnalysisUnavailable;
		return Response;
	}

	const uint64 AnalysisID = NextAnalysisID++;
	TSharedRef<asCAnalysisResult, ESPMode::ThreadSafe> Result = MakeShared<asCAnalysisResult, ESPMode::ThreadSafe>(
		Inputs,
		MoveTemp(Builder),
		AnalysisID);
	Response.Result = Result;
	Response.Coverage = bBodies ? TEXT("bodies") : TEXT("partial-bodies");
	Response.Status = bBodies ? asEAnalysisStatus::Success : asEAnalysisStatus::Partial;
	return Response;
}

END_AS_NAMESPACE
