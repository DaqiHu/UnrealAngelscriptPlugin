/* Maintained Unreal AngelScript fork — tooling query value types. */
#pragma once

#include "as_builder.h"
#include "as_definitions.h"
#include "frontend/AST/as_ast_context.h"
#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Sema/as_call_assessment.h"

BEGIN_AS_NAMESPACE

enum class asEAnalysisStatus : uint8
{
	Success,
	Partial,
	Cancelled,
	InvalidInput,
	InvalidPosition,
	SnapshotMismatch,
	AnalysisUnavailable,
	Unavailable
};

struct asSCancellationToken
{
	const bool* Cancelled = nullptr;
	bool IsCancelled() const { return Cancelled && *Cancelled; }
};

struct asSQueryPosition
{
	FString LogicalSourceKey;
	asUINT ByteOffset = 0;
};

struct asSCursorVisibleSymbol
{
	FString Name;
	FString TypeName;
};

struct asSAnalysisEntityHandle
{
	uint64 AnalysisID = 0;
	asCASTNodeHandle Node;
	bool IsValid() const { return AnalysisID != 0 && Node.IsValid(); }
};

struct asSAnalysisInputs
{
	TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
	asSBuilderOptions Options;
	TArray<TSharedPtr<asCDefinitions, ESPMode::ThreadSafe>> OwnedDefinitions;
	uint64 Identity = 0;
};

class asCAnalysisResult;

struct asSAnalysisResponse
{
	asEAnalysisStatus Status = asEAnalysisStatus::InvalidInput;
	TSharedPtr<const asCAnalysisResult, ESPMode::ThreadSafe> Result;
	FString Coverage;
};

struct asSCompletionItem
{
	FString Name;
	FString Display;
	asCSourceRange Replacement;
	int32 Rank = 0;
};

struct asSCompletionResult
{
	asEAnalysisStatus Status = asEAnalysisStatus::Unavailable;
	TArray<asSCompletionItem> Items;
};

struct asSSignatureParameter
{
	FString Name;
	FString TypeName;
	FString DefaultValue;
	bool bHasDefault = false;
};

struct asSSignatureCandidate
{
	FString Display;
	FString ReturnTypeName;
	TArray<asSSignatureParameter> Parameters;
	bool bViable = false;
	asECandidateRejection Rejection = asECandidateRejection::None;
	TArray<int32> AuthoredToFormal;
	TArray<int32> FormalToSource;
	TArray<asEArgumentSlotKind> FormalSlots;
	int32 Score = 0;
};

struct asSSignatureHelpResult
{
	asEAnalysisStatus Status = asEAnalysisStatus::Unavailable;
	int32 ActiveParameter = INDEX_NONE;
	int32 SelectedIndex = INDEX_NONE;
	FString CallName;
	TArray<asSSignatureCandidate> Signatures;
};

struct asSHoverResult
{
	asEAnalysisStatus Status = asEAnalysisStatus::Unavailable;
	FString Display;
};

enum class asEDefinitionTargetKind : uint8
{
	None,
	SourceTarget,
	NoSourceTarget
};

struct asSDefinitionResult
{
	asEAnalysisStatus Status = asEAnalysisStatus::Unavailable;
	asEDefinitionTargetKind Target = asEDefinitionTargetKind::None;
	asCSourceRange Range;
};

END_AS_NAMESPACE
