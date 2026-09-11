/*
   Maintained Unreal AngelScript fork — snapshot-bound preprocessing range index.
*/

#pragma once

#include "frontend/as_directive_tree.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"

BEGIN_AS_NAMESPACE

enum class asEPreprocessingQueryStatus : uint8
	{
		Succeeded,
		InvalidOwner,
		InvalidRange,
		InvalidRecord,
	};

	enum class asEPreprocessingRecordKind : uint8
	{
		Directive,
		ConditionalBranch,
		Restriction,
	};

	struct asCPreprocessingRecordID
	{
		static constexpr asUINT InvalidValue = MAX_uint32;

		uint64 SnapshotID = 0;
		asUINT Value = InvalidValue;

		bool IsValid() const { return SnapshotID != 0 && Value != InvalidValue; }
		bool operator==(const asCPreprocessingRecordID& Other) const
		{
			return SnapshotID == Other.SnapshotID && Value == Other.Value;
		}
	};

	struct asSPreprocessingRecordEntry
	{
		asCPreprocessingRecordID ID;
		asEPreprocessingRecordKind Kind = asEPreprocessingRecordKind::Directive;
		asCSourceRange Range;
		asUINT EntityIndex = 0;
		asUINT BranchIndex = asSConditionalBranch::InvalidBranchIndex;
	};

	struct asSPreprocessingRecordQuery
	{
		asEPreprocessingQueryStatus Status = asEPreprocessingQueryStatus::InvalidRange;
		TArray<asSPreprocessingRecordEntry> Records;
	};

	struct asSPreprocessingBoolQuery
	{
		asEPreprocessingQueryStatus Status = asEPreprocessingQueryStatus::InvalidRange;
		bool bValue = false;
	};

	struct asSPreprocessingRegionComparison
	{
		asEPreprocessingQueryStatus Status = asEPreprocessingQueryStatus::InvalidRange;
		bool bDifferent = false;
		asUINT LeftBranchIndex = asSConditionalBranch::InvalidBranchIndex;
		asUINT RightBranchIndex = asSConditionalBranch::InvalidBranchIndex;
	};

	struct asSSkippedRangeQuery
	{
		asEPreprocessingQueryStatus Status = asEPreprocessingQueryStatus::InvalidRecord;
		TArray<asSSkippedSourceRange> Ranges;
	};

	class ANGELSCRIPTRUNTIME_API asCPreprocessingRecord
	{
	public:
		bool IsValid() const { return bValid; }
		uint64 GetSnapshotID() const { return SnapshotID; }
		TConstArrayView<asSPreprocessingRecordEntry> GetRecords() const
		{
			return MakeArrayView(Records);
		}

		asSPreprocessingRecordQuery QueryOverlapping(const asCSourceRange& Range) const;
		asSPreprocessingRecordQuery QueryContaining(const asCSourceRange& Range) const;
		asSPreprocessingRegionComparison CompareConditionalRegions(
			const asCSourceRange& Left,
			const asCSourceRange& Right) const;
		asSPreprocessingBoolQuery IntersectsDirective(const asCSourceRange& Range) const;
		asSSkippedRangeQuery GetSkippedRangesForBranch(asCPreprocessingRecordID BranchRecordID) const;

	private:
		friend class asCPreprocessor;
		bool Finalize(const asCDirectiveTree& DirectiveTree);
		asEPreprocessingQueryStatus ValidateRange(const asCSourceRange& Range) const;
		asUINT FindInnermostBranch(const asCSourceRange& Range) const;

		uint64 SnapshotID = 0;
		bool bValid = false;
		TArray<asSPreprocessingRecordEntry> Records;
		TMap<asUINT, TArray<asUINT>> RecordIndicesByFile;
		TArray<asSSkippedSourceRange> SkippedRanges;
	};

END_AS_NAMESPACE

