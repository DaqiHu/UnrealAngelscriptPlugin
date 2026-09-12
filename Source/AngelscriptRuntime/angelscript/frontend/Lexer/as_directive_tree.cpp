/*
   Maintained Unreal AngelScript fork — complete snapshot-local directive tree.
*/

#include "frontend/Lexer/as_directive_tree.h"

BEGIN_AS_NAMESPACE

namespace
	{
		bool IsOwnedOrderedRange(const asCSourceRange& Range, uint64 SnapshotID)
		{
			return Range.IsOrdered() && Range.Begin.SnapshotID == SnapshotID;
		}
	}

	bool asCDirectiveTree::Finalize(uint64 InSnapshotID, bool bInputStructurallyValid)
	{
		SnapshotID = InSnapshotID;
		bool bValid = bInputStructurallyValid && SnapshotID != 0;
		asUINT PreviousDirectiveBegin = 0;
		bool bHasPreviousDirective = false;
		for (const asSDirectiveRecord& Record : DirectiveRecords)
		{
			bValid &= IsOwnedOrderedRange(Record.DirectiveRange, SnapshotID);
			bValid &= !bHasPreviousDirective
				|| PreviousDirectiveBegin <= Record.DirectiveRange.Begin.ByteOffset;
			PreviousDirectiveBegin = Record.DirectiveRange.Begin.ByteOffset;
			bHasPreviousDirective = true;
		}

		for (asUINT ConditionalIndex = 0; ConditionalIndex < static_cast<asUINT>(Conditionals.Num()); ++ConditionalIndex)
		{
			const asSConditionalDirective& Conditional = Conditionals[ConditionalIndex];
			bValid &= IsOwnedOrderedRange(Conditional.OpenDirectiveRange, SnapshotID);
			bValid &= IsOwnedOrderedRange(Conditional.FullRange, SnapshotID);
			bValid &= !Conditional.bTerminated
				|| IsOwnedOrderedRange(Conditional.EndDirectiveRange, SnapshotID);
			for (asUINT BranchIndex : Conditional.BranchIndices)
			{
				bValid &= Branches.IsValidIndex(static_cast<int32>(BranchIndex));
				if (Branches.IsValidIndex(static_cast<int32>(BranchIndex)))
				{
					bValid &= Branches[BranchIndex].ConditionalIndex == ConditionalIndex;
				}
			}
			bValid &= Conditional.TakenBranchIndex == asSConditionalBranch::InvalidBranchIndex
				|| Conditional.BranchIndices.Contains(Conditional.TakenBranchIndex);
		}

		for (const asSConditionalBranch& Branch : Branches)
		{
			bValid &= Conditionals.IsValidIndex(static_cast<int32>(Branch.ConditionalIndex));
			bValid &= IsOwnedOrderedRange(Branch.DirectiveRange, SnapshotID);
			bValid &= IsOwnedOrderedRange(Branch.BodyRange, SnapshotID);
		}

		asUINT PreviousSkippedEnd = 0;
		bool bHasPreviousSkipped = false;
		for (const asSSkippedSourceRange& Skipped : SkippedRanges)
		{
			bValid &= Branches.IsValidIndex(static_cast<int32>(Skipped.BranchIndex));
			bValid &= IsOwnedOrderedRange(Skipped.Range, SnapshotID);
			bValid &= !bHasPreviousSkipped || PreviousSkippedEnd <= Skipped.Range.Begin.ByteOffset;
			PreviousSkippedEnd = Skipped.Range.End.ByteOffset;
			bHasPreviousSkipped = true;
		}

		for (const asSDirectiveCodeChunk& Chunk : CodeChunks)
		{
			bValid &= IsOwnedOrderedRange(Chunk.Range, SnapshotID);
			bValid &= Chunk.BranchIndex == asSConditionalBranch::InvalidBranchIndex
				|| Branches.IsValidIndex(static_cast<int32>(Chunk.BranchIndex));
		}
		for (const asSRestrictionDirective& Restriction : Restrictions)
		{
			bValid &= IsOwnedOrderedRange(Restriction.DirectiveRange, SnapshotID);
			bValid &= IsOwnedOrderedRange(Restriction.PatternRange, SnapshotID);
			bValid &= !Restriction.Pattern.IsEmpty();
			bValid &= DirectiveRecords.IsValidIndex(static_cast<int32>(Restriction.DirectiveRecordIndex));
			bValid &= Restriction.BranchIndex == asSConditionalBranch::InvalidBranchIndex
				|| Branches.IsValidIndex(static_cast<int32>(Restriction.BranchIndex));
		}
		bStructurallyValid = bValid;
		return bStructurallyValid;
	}

END_AS_NAMESPACE
