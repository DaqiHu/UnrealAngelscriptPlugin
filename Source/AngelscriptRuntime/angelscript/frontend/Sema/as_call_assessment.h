/* Maintained Unreal AngelScript fork — non-mutating call candidate assessment. */
#pragma once

#include "frontend/AST/as_ast_fwd.h"
#include "frontend/Sema/as_external_semantics.h"
#include "frontend/AST/as_type.h"
#include "frontend/Basic/as_source_manager.h"

BEGIN_AS_NAMESPACE

class asCDecl;
class asCExpr;
class asCFunctionDecl;
class asCFunctionType;

enum class asECallAssessmentMode : uint8
{
	Complete,
	Incomplete
};

enum class asEArgumentSlotKind : uint8
{
	Unbound,
	Supplied,
	Defaulted,
	UnknownFuture
};

enum class asECandidateRejection : uint8
{
	None,
	InvalidCandidate,
	TooManyArguments,
	UnknownNamedArgument,
	DuplicateNamedArgument,
	PositionalAfterNamed,
	MissingRequiredArgument,
	IncompatibleArgument,
	AmbiguousConversion,
	UnsupportedDefault,
	AccessDenied,
	ReceiverConst,
	ContextRejected
};

struct asSCallArgumentFact
{
	FString Name;
	asCExpr* Value = nullptr;
	asCQualType QualType;
	asEValueCategory ValueCategory = asEValueCategory::PRValue;
	bool bReadOnly = false;
	bool bNull = false;
	bool bInitList = false;
	bool bUnknownFuture = false;
};

struct asSCallAssessmentRequest
{
	TArray<asCDecl*> ScriptCandidates;
	TArray<asSExternalCallable> HostCandidates;
	asCFunctionType* IndirectSignature = nullptr;
	TArray<asSCallArgumentFact> Arguments;
	asCSourceRange Range;
	asECallAssessmentMode Mode = asECallAssessmentMode::Complete;
	bool bPreferNonConstOnTie = false;
};

struct asSCandidateAssessment
{
	asCFunctionDecl* ScriptFunction = nullptr;
	asSExternalCallable HostCallable;
	asCFunctionType* IndirectSignature = nullptr;
	bool bViable = false;
	asECandidateRejection Rejection = asECandidateRejection::None;
	int32 Score = 0;
	TArray<int32> FormalToSource;
	TArray<int32> AuthoredToFormal;
	TArray<asEArgumentSlotKind> FormalSlots;
};

struct asSCallAssessment
{
	TArray<asSCandidateAssessment> Candidates;
	int32 SelectedIndex = INDEX_NONE;
	bool bAmbiguous = false;
	asCFunctionDecl* SelectedScript() const
	{
		return Candidates.IsValidIndex(SelectedIndex) ? Candidates[SelectedIndex].ScriptFunction : nullptr;
	}
	const asSExternalCallable* SelectedHost() const
	{
		if (!Candidates.IsValidIndex(SelectedIndex) || !Candidates[SelectedIndex].HostCallable.Function)
		{
			return nullptr;
		}
		return &Candidates[SelectedIndex].HostCallable;
	}
	const asSCandidateAssessment* Selected() const
	{
		return Candidates.IsValidIndex(SelectedIndex) ? &Candidates[SelectedIndex] : nullptr;
	}
};

END_AS_NAMESPACE
