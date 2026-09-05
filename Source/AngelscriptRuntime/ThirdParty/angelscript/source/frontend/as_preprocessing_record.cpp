/*
   Maintained Unreal AngelScript fork — snapshot-bound preprocessing range index.
*/

#include "frontend/as_preprocessing_record.h"

BEGIN_AS_NAMESPACE

namespace
	{
		bool RangesOverlap(const asCSourceRange& Left, const asCSourceRange& Right)
		{
			return Left.Begin.ByteOffset < Right.End.ByteOffset
				&& Right.Begin.ByteOffset < Left.End.ByteOffset;
		}

		bool RangeContains(const asCSourceRange& Outer, const asCSourceRange& Inner)
		{
			return Outer.Begin.ByteOffset <= Inner.Begin.ByteOffset
				&& Inner.End.ByteOffset <= Outer.End.ByteOffset;
		}
	}

	bool asCPreprocessingRecord::Finalize(const asCDirectiveTree& DirectiveTree)
	{
		SnapshotID = DirectiveTree.GetSnapshotID();
		Records.Reset();
		RecordIndicesByFile.Reset();
		SkippedRanges.Reset();
		bValid = DirectiveTree.IsStructurallyValid() && SnapshotID != 0;

		const TConstArrayView<asSDirectiveRecord> Directives = DirectiveTree.GetDirectiveRecords();
		for (asUINT Index = 0; Index < static_cast<asUINT>(Directives.Num()); ++Index)
		{
			const asSDirectiveRecord& Directive = Directives[Index];
			asSPreprocessingRecordEntry& Entry = Records.AddDefaulted_GetRef();
			Entry.Kind = asEPreprocessingRecordKind::Directive;
			Entry.Range = Directive.DirectiveRange;
			Entry.EntityIndex = Index;
			Entry.BranchIndex = Directive.BranchIndex;
		}

		const TConstArrayView<asSConditionalBranch> Branches = DirectiveTree.GetBranches();
		for (asUINT Index = 0; Index < static_cast<asUINT>(Branches.Num()); ++Index)
		{
			const asSConditionalBranch& Branch = Branches[Index];
			asSPreprocessingRecordEntry& Entry = Records.AddDefaulted_GetRef();
			Entry.Kind = asEPreprocessingRecordKind::ConditionalBranch;
			Entry.Range = Branch.BodyRange;
			Entry.EntityIndex = Index;
			Entry.BranchIndex = Index;
		}

		const TConstArrayView<asSRestrictionDirective> Restrictions = DirectiveTree.GetRestrictions();
		for (asUINT Index = 0; Index < static_cast<asUINT>(Restrictions.Num()); ++Index)
		{
			const asSRestrictionDirective& Restriction = Restrictions[Index];
			asSPreprocessingRecordEntry& Entry = Records.AddDefaulted_GetRef();
			Entry.Kind = asEPreprocessingRecordKind::Restriction;
			Entry.Range = Restriction.DirectiveRange;
			Entry.EntityIndex = Index;
			Entry.BranchIndex = Restriction.BranchIndex;
		}

		Records.Sort([](const asSPreprocessingRecordEntry& Left, const asSPreprocessingRecordEntry& Right)
		{
			if (Left.Range.Begin.FileID.Value != Right.Range.Begin.FileID.Value)
			{
				return Left.Range.Begin.FileID.Value < Right.Range.Begin.FileID.Value;
			}
			if (Left.Range.Begin.ByteOffset != Right.Range.Begin.ByteOffset)
			{
				return Left.Range.Begin.ByteOffset < Right.Range.Begin.ByteOffset;
			}
			if (Left.Range.End.ByteOffset != Right.Range.End.ByteOffset)
			{
				return Left.Range.End.ByteOffset < Right.Range.End.ByteOffset;
			}
			if (Left.Kind != Right.Kind)
			{
				return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind);
			}
			return Left.EntityIndex < Right.EntityIndex;
		});

		for (asUINT Index = 0; Index < static_cast<asUINT>(Records.Num()); ++Index)
		{
			asSPreprocessingRecordEntry& Entry = Records[Index];
			bValid &= Entry.Range.IsOrdered() && Entry.Range.Begin.SnapshotID == SnapshotID;
			Entry.ID = asCPreprocessingRecordID{SnapshotID, Index};
			RecordIndicesByFile.FindOrAdd(Entry.Range.Begin.FileID.Value).Add(Index);
		}

		SkippedRanges.Append(DirectiveTree.GetSkippedRanges());
		for (const asSSkippedSourceRange& Skipped : SkippedRanges)
		{
			bValid &= Skipped.Range.IsOrdered() && Skipped.Range.Begin.SnapshotID == SnapshotID;
			bValid &= Branches.IsValidIndex(static_cast<int32>(Skipped.BranchIndex));
		}
		return bValid;
	}

	asEPreprocessingQueryStatus asCPreprocessingRecord::ValidateRange(const asCSourceRange& Range) const
	{
		if (!Range.HasConsistentOwner() || Range.Begin.SnapshotID != SnapshotID)
		{
			return asEPreprocessingQueryStatus::InvalidOwner;
		}
		if (!bValid || !Range.IsOrdered())
		{
			return asEPreprocessingQueryStatus::InvalidRange;
		}
		return asEPreprocessingQueryStatus::Succeeded;
	}

	asSPreprocessingRecordQuery asCPreprocessingRecord::QueryOverlapping(
		const asCSourceRange& Range) const
	{
		asSPreprocessingRecordQuery Result;
		Result.Status = ValidateRange(Range);
		if (Result.Status != asEPreprocessingQueryStatus::Succeeded) { return Result; }
		const TArray<asUINT>* FileIndices = RecordIndicesByFile.Find(Range.Begin.FileID.Value);
		if (FileIndices == nullptr) { return Result; }
		for (asUINT Index : *FileIndices)
		{
			const asSPreprocessingRecordEntry& Entry = Records[Index];
			if (RangesOverlap(Entry.Range, Range)) { Result.Records.Add(Entry); }
		}
		return Result;
	}

	asSPreprocessingRecordQuery asCPreprocessingRecord::QueryContaining(
		const asCSourceRange& Range) const
	{
		asSPreprocessingRecordQuery Result;
		Result.Status = ValidateRange(Range);
		if (Result.Status != asEPreprocessingQueryStatus::Succeeded) { return Result; }
		const TArray<asUINT>* FileIndices = RecordIndicesByFile.Find(Range.Begin.FileID.Value);
		if (FileIndices == nullptr) { return Result; }
		for (asUINT Index : *FileIndices)
		{
			const asSPreprocessingRecordEntry& Entry = Records[Index];
			if (RangeContains(Entry.Range, Range)) { Result.Records.Add(Entry); }
		}
		return Result;
	}

	asUINT asCPreprocessingRecord::FindInnermostBranch(const asCSourceRange& Range) const
	{
		asUINT BestBranch = asSConditionalBranch::InvalidBranchIndex;
		asUINT BestLength = MAX_uint32;
		const TArray<asUINT>* FileIndices = RecordIndicesByFile.Find(Range.Begin.FileID.Value);
		if (FileIndices == nullptr) { return BestBranch; }
		for (asUINT Index : *FileIndices)
		{
			const asSPreprocessingRecordEntry& Entry = Records[Index];
			if (Entry.Kind == asEPreprocessingRecordKind::ConditionalBranch
				&& RangeContains(Entry.Range, Range)
				&& Entry.Range.Length() <= BestLength)
			{
				BestBranch = Entry.EntityIndex;
				BestLength = Entry.Range.Length();
			}
		}
		return BestBranch;
	}

	asSPreprocessingRegionComparison asCPreprocessingRecord::CompareConditionalRegions(
		const asCSourceRange& Left,
		const asCSourceRange& Right) const
	{
		asSPreprocessingRegionComparison Result;
		Result.Status = ValidateRange(Left);
		if (Result.Status != asEPreprocessingQueryStatus::Succeeded) { return Result; }
		Result.Status = ValidateRange(Right);
		if (Result.Status != asEPreprocessingQueryStatus::Succeeded) { return Result; }
		Result.LeftBranchIndex = FindInnermostBranch(Left);
		Result.RightBranchIndex = FindInnermostBranch(Right);
		Result.bDifferent = Left.Begin.FileID != Right.Begin.FileID
			|| Result.LeftBranchIndex != Result.RightBranchIndex;
		return Result;
	}

	asSPreprocessingBoolQuery asCPreprocessingRecord::IntersectsDirective(
		const asCSourceRange& Range) const
	{
		asSPreprocessingBoolQuery Result;
		Result.Status = ValidateRange(Range);
		if (Result.Status != asEPreprocessingQueryStatus::Succeeded) { return Result; }
		const TArray<asUINT>* FileIndices = RecordIndicesByFile.Find(Range.Begin.FileID.Value);
		if (FileIndices == nullptr) { return Result; }
		for (asUINT Index : *FileIndices)
		{
			const asSPreprocessingRecordEntry& Entry = Records[Index];
			if (Entry.Kind == asEPreprocessingRecordKind::Directive
				&& RangesOverlap(Entry.Range, Range))
			{
				Result.bValue = true;
				break;
			}
		}
		return Result;
	}

	asSSkippedRangeQuery asCPreprocessingRecord::GetSkippedRangesForBranch(
		asCPreprocessingRecordID BranchRecordID) const
	{
		asSSkippedRangeQuery Result;
		if (BranchRecordID.SnapshotID != SnapshotID)
		{
			Result.Status = asEPreprocessingQueryStatus::InvalidOwner;
			return Result;
		}
		if (!BranchRecordID.IsValid()
			|| !Records.IsValidIndex(static_cast<int32>(BranchRecordID.Value))
			|| Records[BranchRecordID.Value].Kind != asEPreprocessingRecordKind::ConditionalBranch)
		{
			Result.Status = asEPreprocessingQueryStatus::InvalidRecord;
			return Result;
		}
		Result.Status = asEPreprocessingQueryStatus::Succeeded;
		const asUINT BranchIndex = Records[BranchRecordID.Value].EntityIndex;
		for (const asSSkippedSourceRange& Skipped : SkippedRanges)
		{
			if (Skipped.BranchIndex == BranchIndex) { Result.Ranges.Add(Skipped); }
		}
		return Result;
	}

END_AS_NAMESPACE

