/*
   Maintained Unreal AngelScript fork — complete snapshot-local directive tree.
*/

#pragma once

#include "frontend/Basic/as_source_location.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"

BEGIN_AS_NAMESPACE

enum class asEDirectiveKind : uint8
	{
		Invalid,
#define AS_DIRECTIVE(Name, Spelling) Name,
#include "frontend/Lexer/as_directive_kinds.def"
#undef AS_DIRECTIVE
	};

	inline const TCHAR* asGetDirectiveKindName(asEDirectiveKind Kind)
	{
		switch (Kind)
		{
#define AS_DIRECTIVE(Name, Spelling) case asEDirectiveKind::Name: return TEXT(#Name);
#include "frontend/Lexer/as_directive_kinds.def"
#undef AS_DIRECTIVE
		default: return TEXT("Invalid");
		}
	}

	enum class asEConditionEvaluation : uint8
	{
		NotEvaluated,
		False,
		True,
		Invalid,
	};

	struct asSConditionalBranch
	{
		static constexpr asUINT InvalidBranchIndex = MAX_uint32;

		asEDirectiveKind IntroducerKind = asEDirectiveKind::Invalid;
		asCSourceRange DirectiveRange;
		asCSourceRange ConditionRange;
		asCSourceRange BodyRange;
		asEConditionEvaluation Evaluation = asEConditionEvaluation::NotEvaluated;
		asUINT ConditionalIndex = InvalidBranchIndex;
		asUINT ParentBranchIndex = InvalidBranchIndex;
		asUINT Depth = 0;
		bool bTaken = false;
	};

	struct asSConditionalDirective
	{
		asUINT ParentConditionalIndex = asSConditionalBranch::InvalidBranchIndex;
		asUINT ParentBranchIndex = asSConditionalBranch::InvalidBranchIndex;
		asUINT TakenBranchIndex = asSConditionalBranch::InvalidBranchIndex;
		asCSourceRange OpenDirectiveRange;
		asCSourceRange EndDirectiveRange;
		asCSourceRange FullRange;
		TArray<asUINT> BranchIndices;
		bool bTerminated = false;
	};

	struct asSDirectiveRecord
	{
		asEDirectiveKind Kind = asEDirectiveKind::Invalid;
		asCSourceRange DirectiveRange;
		asCSourceRange ArgumentRange;
		asUINT ConditionalIndex = asSConditionalBranch::InvalidBranchIndex;
		asUINT BranchIndex = asSConditionalBranch::InvalidBranchIndex;
		bool bTerminator = false;
	};

	struct asSDirectiveCodeChunk
	{
		asCSourceRange Range;
		asUINT BranchIndex = asSConditionalBranch::InvalidBranchIndex;
		bool bActive = true;
	};

	struct asSSkippedSourceRange
	{
		asCSourceRange Range;
		asUINT BranchIndex = asSConditionalBranch::InvalidBranchIndex;
	};

	enum class asERestrictionPolicy : uint8
	{
		Allow,
		Disallow,
	};

	struct asSRestrictionDirective
	{
		asERestrictionPolicy Policy = asERestrictionPolicy::Allow;
		FString Pattern;
		asCSourceRange DirectiveRange;
		asCSourceRange PatternRange;
		asUINT DirectiveRecordIndex = asSConditionalBranch::InvalidBranchIndex;
		asUINT BranchIndex = asSConditionalBranch::InvalidBranchIndex;
		bool bActive = false;
	};

	class ANGELSCRIPTRUNTIME_API asCDirectiveTree
	{
	public:
		bool IsStructurallyValid() const { return bStructurallyValid; }
		uint64 GetSnapshotID() const { return SnapshotID; }
		TConstArrayView<asSConditionalDirective> GetConditionals() const { return MakeArrayView(Conditionals); }
		TConstArrayView<asSConditionalBranch> GetBranches() const { return MakeArrayView(Branches); }
		TConstArrayView<asSDirectiveRecord> GetDirectiveRecords() const { return MakeArrayView(DirectiveRecords); }
		TConstArrayView<asSDirectiveCodeChunk> GetCodeChunks() const { return MakeArrayView(CodeChunks); }
		TConstArrayView<asSSkippedSourceRange> GetSkippedRanges() const { return MakeArrayView(SkippedRanges); }
		TConstArrayView<asSRestrictionDirective> GetRestrictions() const { return MakeArrayView(Restrictions); }

	private:
		friend class asCPreprocessor;
		bool Finalize(uint64 InSnapshotID, bool bInputStructurallyValid);

		uint64 SnapshotID = 0;
		bool bStructurallyValid = false;
		TArray<asSConditionalDirective> Conditionals;
		TArray<asSConditionalBranch> Branches;
		TArray<asSDirectiveRecord> DirectiveRecords;
		TArray<asSDirectiveCodeChunk> CodeChunks;
		TArray<asSSkippedSourceRange> SkippedRanges;
		TArray<asSRestrictionDirective> Restrictions;
	};

END_AS_NAMESPACE
