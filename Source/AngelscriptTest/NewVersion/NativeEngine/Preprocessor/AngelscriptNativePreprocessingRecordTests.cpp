#include "CQTest.h"
#include "NativePreprocessorTestSupport.h"

#include "frontend/as_preprocessing_record.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

using AngelscriptNativePreprocessorTest::FPreprocessorRun;
using AngelscriptNativePreprocessorTest::MakeFlags;

namespace PreprocessingRecordTests
{

static const asSPreprocessingRecordEntry* FindRecord(
		TConstArrayView<asSPreprocessingRecordEntry> Records,
		asEPreprocessingRecordKind Kind,
		asUINT EntityIndex)
	{
		for (const asSPreprocessingRecordEntry& Record : Records)
		{
			if (Record.Kind == Kind && Record.EntityIndex == EntityIndex)
			{
				return &Record;
			}
		}
		return nullptr;
	}

	TEST_CLASS_WITH_FLAGS(
		PreprocessorRecord,
		"Angelscript.UnitTest.NativeEngine",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	{
		TEST_METHOD(OverlapAndContainmentReturnConditionalAndDirectiveRecords)
		{
			const FString Source =
				TEXT("#if FEATURE\nint Active;\n#else\nint Inactive;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FEATURE"), true}}));
			const asUINT ActiveBegin = static_cast<asUINT>(Source.Find(TEXT("Active")));
			const AS_NAMESPACE_QUALIFIER asCSourceRange ActiveRange = Run.SourceManager.MakeRange(
				Run.FileID, ActiveBegin, ActiveBegin + 6);

			const AS_NAMESPACE_QUALIFIER asSPreprocessingRecordQuery Containing =
				Run.Result.PreprocessingRecord.QueryContaining(ActiveRange);
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Containing.Status));
			ASSERT_THAT(IsNotNull(FindRecord(
				Containing.Records,
				asEPreprocessingRecordKind::ConditionalBranch,
				0)));

			const AS_NAMESPACE_QUALIFIER asCSourceRange IfRange = Run.SourceManager.MakeRange(
				Run.FileID, 0, static_cast<asUINT>(Source.Find(TEXT("\n"))));
			const AS_NAMESPACE_QUALIFIER asSPreprocessingRecordQuery Overlapping =
				Run.Result.PreprocessingRecord.QueryOverlapping(IfRange);
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Overlapping.Status));
			ASSERT_THAT(IsNotNull(FindRecord(
				Overlapping.Records,
				asEPreprocessingRecordKind::Directive,
				0)));
		}

		TEST_METHOD(QueryResultsRemainInAuthoredSourceOrder)
		{
			const FString Source =
				TEXT("#restrict usage allow Runtime\n#if FEATURE\nint Value;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FEATURE"), true}}));
			const AS_NAMESPACE_QUALIFIER asCSourceRange WholeFile = Run.SourceManager.MakeRange(
				Run.FileID, 0, Run.RawTokens.Last().Range.Begin.ByteOffset);
			const AS_NAMESPACE_QUALIFIER asSPreprocessingRecordQuery Query =
				Run.Result.PreprocessingRecord.QueryOverlapping(WholeFile);

			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Query.Status));
			ASSERT_THAT(IsTrue(Query.Records.Num() >= 4));
			for (int32 Index = 1; Index < Query.Records.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(
					Query.Records[Index - 1].Range.Begin.ByteOffset
					<= Query.Records[Index].Range.Begin.ByteOffset));
			}
		}

		TEST_METHOD(DifferentRegionComparisonUsesInnermostBranch)
		{
			const FString Source =
				TEXT("#if FEATURE\nint First; int Same;\n#else\nint Other;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FEATURE"), true}}));
			auto WordRange = [&Run, &Source](const TCHAR* Word)
			{
				const asUINT Begin = static_cast<asUINT>(Source.Find(Word));
				return Run.SourceManager.MakeRange(
					Run.FileID, Begin, Begin + static_cast<asUINT>(FCString::Strlen(Word)));
			};

			const AS_NAMESPACE_QUALIFIER asSPreprocessingRegionComparison Same =
				Run.Result.PreprocessingRecord.CompareConditionalRegions(
					WordRange(TEXT("First")), WordRange(TEXT("Same")));
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Same.Status));
			ASSERT_THAT(IsFalse(Same.bDifferent));

			const AS_NAMESPACE_QUALIFIER asSPreprocessingRegionComparison Different =
				Run.Result.PreprocessingRecord.CompareConditionalRegions(
					WordRange(TEXT("First")), WordRange(TEXT("Other")));
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Different.Status));
			ASSERT_THAT(IsTrue(Different.bDifferent));
		}

		TEST_METHOD(DirectiveIntersectionDetectsOnlyAuthoredDirectiveRanges)
		{
			const FString Source = TEXT("#if FEATURE\nint Value;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FEATURE"), true}}));
			const asUINT ValueBegin = static_cast<asUINT>(Source.Find(TEXT("Value")));
			const AS_NAMESPACE_QUALIFIER asSPreprocessingBoolQuery Body =
				Run.Result.PreprocessingRecord.IntersectsDirective(Run.SourceManager.MakeRange(
					Run.FileID, ValueBegin, ValueBegin + 5));
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Body.Status));
			ASSERT_THAT(IsFalse(Body.bValue));

			const AS_NAMESPACE_QUALIFIER asSPreprocessingBoolQuery Boundary =
				Run.Result.PreprocessingRecord.IntersectsDirective(Run.SourceManager.MakeRange(
					Run.FileID, 0, ValueBegin));
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Boundary.Status));
			ASSERT_THAT(IsTrue(Boundary.bValue));
		}

		TEST_METHOD(SkippedRangesRetainTheirOwningBranchRecord)
		{
			const FString Source =
				TEXT("#if FEATURE\nint Active;\n#else\nint Inactive;\n#endif\n");
			const FPreprocessorRun Run(Source, MakeFlags({{TEXT("FEATURE"), true}}));
			const asSPreprocessingRecordEntry* InactiveBranch = FindRecord(
				Run.Result.PreprocessingRecord.GetRecords(),
				asEPreprocessingRecordKind::ConditionalBranch,
				1);
			ASSERT_THAT(IsNotNull(InactiveBranch));

			const AS_NAMESPACE_QUALIFIER asSSkippedRangeQuery Query =
				Run.Result.PreprocessingRecord.GetSkippedRangesForBranch(InactiveBranch->ID);
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::Succeeded, Query.Status));
			ASSERT_THAT(AreEqual(1, Query.Ranges.Num()));
			ASSERT_THAT(AreEqual(1u, Query.Ranges[0].BranchIndex));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Source.Find(TEXT("\nint Inactive;"))),
				Query.Ranges[0].Range.Begin.ByteOffset));
		}

		TEST_METHOD(ForeignSnapshotCoordinatesFailDespiteReusedNumericIDs)
		{
			const FString Source = TEXT("#if FEATURE\nint Value;\n#endif\n");
			const FPreprocessorRun First(Source, MakeFlags({{TEXT("FEATURE"), false}}));
			const FPreprocessorRun Second(Source, MakeFlags({{TEXT("FEATURE"), false}}));
			const asSPreprocessingRecordEntry* FirstBranch = FindRecord(
				First.Result.PreprocessingRecord.GetRecords(),
				asEPreprocessingRecordKind::ConditionalBranch,
				0);
			const asSPreprocessingRecordEntry* SecondBranch = FindRecord(
				Second.Result.PreprocessingRecord.GetRecords(),
				asEPreprocessingRecordKind::ConditionalBranch,
				0);
			ASSERT_THAT(IsNotNull(FirstBranch));
			ASSERT_THAT(IsNotNull(SecondBranch));
			ASSERT_THAT(AreEqual(FirstBranch->ID.Value, SecondBranch->ID.Value));
			ASSERT_THAT(IsTrue(FirstBranch->ID.SnapshotID != SecondBranch->ID.SnapshotID));

			const AS_NAMESPACE_QUALIFIER asSPreprocessingRecordQuery ForeignRange =
				Second.Result.PreprocessingRecord.QueryOverlapping(FirstBranch->Range);
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::InvalidOwner, ForeignRange.Status));
			ASSERT_THAT(AreEqual(0, ForeignRange.Records.Num()));

			const AS_NAMESPACE_QUALIFIER asSSkippedRangeQuery ForeignID =
				Second.Result.PreprocessingRecord.GetSkippedRangesForBranch(FirstBranch->ID);
			ASSERT_THAT(AreEqual(asEPreprocessingQueryStatus::InvalidOwner, ForeignID.Status));
			ASSERT_THAT(AreEqual(0, ForeignID.Ranges.Num()));
		}
	};
}

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
