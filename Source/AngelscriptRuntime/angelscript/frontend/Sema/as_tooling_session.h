/* Maintained Unreal AngelScript fork — owned tooling analysis session. */
#pragma once

#include "frontend/Compile/as_analysis_result.h"
#include "frontend/Sema/as_cursor_context.h"

BEGIN_AS_NAMESPACE

class asCBuilder;

class ANGELSCRIPTRUNTIME_API asCToolingSession
{
public:
	explicit asCToolingSession(TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> Inputs);

	asSAnalysisResponse Analyze(const asSCancellationToken& Cancel);
	asSCursorContext PrepareCursor(asSQueryPosition Position, const asSCancellationToken& Cancel) const;
	asSCompletionResult Complete(asSQueryPosition Position, const asSCancellationToken& Cancel);
	asSSignatureHelpResult GetSignatureHelp(asSQueryPosition Position, const asSCancellationToken& Cancel);
	asSHoverResult GetHover(const asCAnalysisResult& Result, asSQueryPosition Position, const asSCancellationToken& Cancel);
	asSDefinitionResult FindDefinition(const asCAnalysisResult& Result, asSQueryPosition Position, const asSCancellationToken& Cancel);

	const asSAnalysisInputs& GetInputs() const { return Inputs.Get(); }
#if WITH_ANGELSCRIPT_TESTS
	enum class asEToolingCancelCheckpoint : uint8
	{
		None,
		AfterDeclarations,
		DuringCursorParse
	};

	void DebugPlantForeignRangeOnNextAnalyze();
	void DebugFailNextDeclarationStage();
	void DebugCancelAt(asEToolingCancelCheckpoint Checkpoint, bool* Flag);
#endif

private:
	asSCursorContext PrepareCursor(asSQueryPosition Position, const asSCancellationToken& Cancel, asCBuilder* Prepared) const;
	bool ValidateInputs() const;
#if WITH_ANGELSCRIPT_TESTS
	bool ConsumeCancelCheckpoint(asEToolingCancelCheckpoint Checkpoint) const;
	bool bDebugPlantForeignRange = false;
	mutable bool bDebugFailDeclarations = false;
	mutable asEToolingCancelCheckpoint DebugCheckpoint = asEToolingCancelCheckpoint::None;
	mutable bool* DebugCancelFlag = nullptr;
#endif

	TSharedRef<const asSAnalysisInputs, ESPMode::ThreadSafe> Inputs;
	uint64 NextAnalysisID = 1;
};

END_AS_NAMESPACE
