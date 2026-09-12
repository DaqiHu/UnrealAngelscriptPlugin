#include "CQTest.h"
#include "NativeEngineTestSupport.h"

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_source_location.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/Basic/as_source_provenance.h"
#include "frontend/Basic/as_source_snapshot.h"

#include "Async/ParallelFor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> MakeSnapshot(
		FStringView LogicalPath,
		FStringView SourceText,
		AS_NAMESPACE_QUALIFIER asCSourceFileID& OutFileID)
	{
		const AngelscriptNativeEngineTest::FSourceInput Input =
			AngelscriptNativeEngineTest::FSourceInput::FromText(LogicalPath, SourceText);
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		OutFileID = Snapshot->AddFile(Input.GetLogicalPath(), Input.GetUtf8Bytes());
		Snapshot->Freeze();
		return Snapshot;
	}
}

TEST_CLASS_WITH_FLAGS(
	SourceDiagnostics,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(HalfOpenRangeSelectsExactUtf8Bytes)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Utf8.as"), TEXT("A\u03c0BC"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);

		const AS_NAMESPACE_QUALIFIER asCSourceRange Range = Manager.MakeRange(FileID, 1, 3);
		TConstArrayView<uint8> Slice;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(Range, Slice), TEXT("The UTF-8 byte range should resolve.")));
		const uint8 Expected[] = {0xCF, 0x80};
		ASSERT_THAT(AreEqual(2, Slice.Num()));
		ASSERT_THAT(IsTrue(
			FMemory::Memcmp(Slice.GetData(), Expected, UE_ARRAY_COUNT(Expected)) == 0,
			TEXT("[1, 3) should select exactly the two UTF-8 bytes for pi.")));
	}

	TEST_METHOD(InvalidRangesAreRejected)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Invalid.as"), TEXT("abc"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);

		TConstArrayView<uint8> Slice;
		ASSERT_THAT(IsFalse(Manager.TryGetBytes(Manager.MakeRange(FileID, 2, 1), Slice)));
		ASSERT_THAT(IsFalse(Manager.TryGetBytes(Manager.MakeRange(FileID, 0, 4), Slice)));
		ASSERT_THAT(IsFalse(Manager.TryGetBytes(AS_NAMESPACE_QUALIFIER asCSourceRange(), Slice)));
	}

	TEST_METHOD(SnapshotIdentityRejectsMixedLocations)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FirstFileID;
		AS_NAMESPACE_QUALIFIER asCSourceFileID SecondFileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> FirstSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Same.as"), TEXT("first"), FirstFileID);
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> SecondSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Same.as"), TEXT("first"), SecondFileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager FirstManager(FirstSnapshot);
		const AS_NAMESPACE_QUALIFIER asCSourceManager SecondManager(SecondSnapshot);

		ASSERT_THAT(AreEqual(FirstFileID.Value, SecondFileID.Value));
		const AS_NAMESPACE_QUALIFIER asCSourceRange Mixed(
			FirstManager.MakeLocation(FirstFileID, 0),
			SecondManager.MakeLocation(SecondFileID, 1));
		TConstArrayView<uint8> Slice;
		ASSERT_THAT(IsFalse(FirstManager.TryGetBytes(Mixed, Slice)));
		ASSERT_THAT(IsFalse(SecondManager.TryGetBytes(Mixed, Slice)));
	}

	TEST_METHOD(StableAnchorRelocatesWithoutSnapshotLocalIdentity)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FirstFileID;
		AS_NAMESPACE_QUALIFIER asCSourceFileID SecondFileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> FirstSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Stable.as"), TEXT("value"), FirstFileID);
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> SecondSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Stable.as"), TEXT("value"), SecondFileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager FirstManager(FirstSnapshot);
		const AS_NAMESPACE_QUALIFIER asCSourceManager SecondManager(SecondSnapshot);

		AS_NAMESPACE_QUALIFIER asSStableSourceAnchor Anchor;
		ASSERT_THAT(IsTrue(FirstManager.TryCreateStableAnchor(
			FirstManager.MakeRange(FirstFileID, 1, 4), Anchor)));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Game/Stable.as")), Anchor.LogicalSourceKey));
		ASSERT_THAT(IsTrue(Anchor.ContentRevision != 0, TEXT("The anchor should retain durable content identity.")));

		AS_NAMESPACE_QUALIFIER asCSourceRange Relocated;
		ASSERT_THAT(IsTrue(SecondManager.TryRelocateStableAnchor(Anchor, Relocated)));
		ASSERT_THAT(AreEqual(1u, Relocated.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(4u, Relocated.End.ByteOffset));
		ASSERT_THAT(IsTrue(Relocated.Begin.SnapshotID != FirstManager.GetSnapshotID()));
	}

	TEST_METHOD(LineMapIsLazyAndUsesUtf8ByteColumns)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Lines.as"), TEXT("one\n\u03c0x\n"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);

		ASSERT_THAT(IsFalse(Manager.HasMaterializedLineMap(FileID)));
		AS_NAMESPACE_QUALIFIER asSSourceLineColumn LineColumn;
		ASSERT_THAT(IsTrue(Manager.TryGetLineColumn(Manager.MakeLocation(FileID, 6), LineColumn)));
		ASSERT_THAT(AreEqual(2u, LineColumn.Line));
		ASSERT_THAT(AreEqual(3u, LineColumn.Column));
		ASSERT_THAT(IsTrue(Manager.HasMaterializedLineMap(FileID)));
	}

	TEST_METHOD(ConcurrentLineQueriesPublishOneEquivalentMap)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Parallel.as"), TEXT("zero\none\ntwo"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);
		TArray<AS_NAMESPACE_QUALIFIER asSSourceLineColumn> Results;
		Results.SetNum(64);

		ParallelFor(Results.Num(), [&Manager, FileID, &Results](int32 Index)
		{
			Manager.TryGetLineColumn(Manager.MakeLocation(FileID, 9), Results[Index]);
		});

		for (const AS_NAMESPACE_QUALIFIER asSSourceLineColumn& Result : Results)
		{
			ASSERT_THAT(AreEqual(3u, Result.Line));
			ASSERT_THAT(AreEqual(1u, Result.Column));
		}
	}

	TEST_METHOD(OriginQueriesReturnOverlapsAndParentChain)
	{
		const AngelscriptNativeEngineTest::FSourceInput Input =
			AngelscriptNativeEngineTest::FSourceInput::FromText(
				TEXT("/Angelscript/Game/Origin.as"), TEXT("abcdef"));
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileID = Snapshot->AddFile(
			Input.GetLogicalPath(), Input.GetUtf8Bytes());
		const AS_NAMESPACE_QUALIFIER asCSourceManager ConstructionManager(Snapshot);

		AS_NAMESPACE_QUALIFIER asSSourceOriginRecord Direct;
		Direct.Kind = AS_NAMESPACE_QUALIFIER asESourceOriginKind::Direct;
		Direct.ProducedRange = ConstructionManager.MakeRange(FileID, 0, 6);
		Direct.Producer = TEXT("spelled");
		const AS_NAMESPACE_QUALIFIER asCSourceOriginID DirectID = Snapshot->AddOrigin(Direct);

		AS_NAMESPACE_QUALIFIER asSSourceOriginRecord Synthetic;
		Synthetic.Kind = AS_NAMESPACE_QUALIFIER asESourceOriginKind::Synthetic;
		Synthetic.ProducedRange = ConstructionManager.MakeRange(FileID, 2, 4);
		Synthetic.ParentRange = ConstructionManager.MakeRange(FileID, 1, 3);
		Synthetic.ParentOrigin = DirectID;
		Synthetic.Producer = TEXT("test-transform");
		const AS_NAMESPACE_QUALIFIER asCSourceOriginID SyntheticID = Snapshot->AddOrigin(Synthetic);
		ASSERT_THAT(IsTrue(DirectID.IsValid()));
		ASSERT_THAT(IsTrue(SyntheticID.IsValid()));
		Snapshot->Freeze();

		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);
		TArray<AS_NAMESPACE_QUALIFIER asSSourceOriginRecord> Overlaps;
		ASSERT_THAT(IsTrue(Manager.FindOrigins(Manager.MakeRange(FileID, 2, 3), Overlaps)));
		ASSERT_THAT(AreEqual(2, Overlaps.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asESourceOriginKind::Direct, Overlaps[0].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asESourceOriginKind::Synthetic, Overlaps[1].Kind));

		TArray<AS_NAMESPACE_QUALIFIER asSSourceOriginRecord> Chain;
		ASSERT_THAT(IsTrue(Manager.TryResolveOriginChain(SyntheticID, Chain)));
		ASSERT_THAT(AreEqual(2, Chain.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("test-transform")), Chain[0].Producer));
		ASSERT_THAT(AreEqual(FString(TEXT("spelled")), Chain[1].Producer));

		AS_NAMESPACE_QUALIFIER asCSourceFileID OtherFileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> OtherSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Origin.as"), TEXT("abcdef"), OtherFileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager OtherManager(OtherSnapshot);
		ASSERT_THAT(IsFalse(OtherManager.TryResolveOriginChain(SyntheticID, Chain)));
	}

	TEST_METHOD(StableAnchorRevisionMismatchFailsRelocation)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID OriginalFileID;
		AS_NAMESPACE_QUALIFIER asCSourceFileID ChangedFileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> OriginalSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Revision.as"), TEXT("before"), OriginalFileID);
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> ChangedSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Revision.as"), TEXT("after"), ChangedFileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager OriginalManager(OriginalSnapshot);
		const AS_NAMESPACE_QUALIFIER asCSourceManager ChangedManager(ChangedSnapshot);

		AS_NAMESPACE_QUALIFIER asSStableSourceAnchor Anchor;
		ASSERT_THAT(IsTrue(OriginalManager.TryCreateStableAnchor(
			OriginalManager.MakeRange(OriginalFileID, 0, 6), Anchor)));
		AS_NAMESPACE_QUALIFIER asCSourceRange Relocated;
		ASSERT_THAT(IsFalse(ChangedManager.TryRelocateStableAnchor(Anchor, Relocated)));
		ASSERT_THAT(IsFalse(Relocated.Begin.IsValid()));
	}

	TEST_METHOD(StructuredDiagnosticsDeferRenderingAndKeepTypedArguments)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Diagnostic.as"), TEXT("bad value"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Diagnostics.CreateFragment(TEXT("parser/body-0"));

		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Record;
		Record.DiagnosticID = 4101;
		Record.Severity = AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error;
		Record.PrimaryRange = Manager.MakeRange(FileID, 0, 3);
		Record.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("value")));
		Record.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromSigned(42));
		Record.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromBoolean(true));
		ASSERT_THAT(IsTrue(Fragment.Report(MoveTemp(Record))));
		ASSERT_THAT(IsTrue(Diagnostics.Submit(MoveTemp(Fragment))));

		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records = Consumer.GetRecords();
		ASSERT_THAT(AreEqual(1, Records.Num()));
		ASSERT_THAT(AreEqual(4101u, Records[0].DiagnosticID));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error, Records[0].Severity));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Text, Records[0].Arguments[0].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Signed, Records[0].Arguments[1].Kind));
		ASSERT_THAT(AreEqual(42ll, Records[0].Arguments[1].SignedValue));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Boolean, Records[0].Arguments[2].Kind));

		const FString Rendered = AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::Render(
			TEXT("unexpected {0} {1} enabled={2}"), Records[0]);
		ASSERT_THAT(AreEqual(FString(TEXT("unexpected value 42 enabled=true")), Rendered));
	}

	TEST_METHOD(RelatedRangesAndFixItsRemainSourceBound)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		AS_NAMESPACE_QUALIFIER asCSourceFileID ForeignFileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Fix.as"), TEXT("old name"), FileID);
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> ForeignSnapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Fix.as"), TEXT("old name"), ForeignFileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);
		const AS_NAMESPACE_QUALIFIER asCSourceManager ForeignManager(ForeignSnapshot);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);

		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Valid;
		Valid.DiagnosticID = 4201;
		Valid.Severity = AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Warning;
		Valid.PrimaryRange = Manager.MakeRange(FileID, 0, 3);
		Valid.RelatedRanges.Add(Manager.MakeRange(FileID, 4, 8));
		Valid.FixIts.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticFixIt::FromText(
			Manager.MakeRange(FileID, 0, 3), TEXT("new")));
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment ValidFragment = Diagnostics.CreateFragment(TEXT("sema/name"));
		ASSERT_THAT(IsTrue(ValidFragment.Report(MoveTemp(Valid))));
		ASSERT_THAT(IsTrue(Diagnostics.Submit(MoveTemp(ValidFragment))));

		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Invalid;
		Invalid.DiagnosticID = 4202;
		Invalid.Severity = AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error;
		Invalid.PrimaryRange = Manager.MakeRange(FileID, 0, 3);
		Invalid.RelatedRanges.Add(ForeignManager.MakeRange(ForeignFileID, 0, 3));
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment InvalidFragment = Diagnostics.CreateFragment(TEXT("sema/foreign"));
		ASSERT_THAT(IsFalse(InvalidFragment.Report(MoveTemp(Invalid))));

		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords().Num()));
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords()[0].RelatedRanges.Num()));
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords()[0].FixIts.Num()));
		const uint8 Expected[] = {'n', 'e', 'w'};
		ASSERT_THAT(IsTrue(FMemory::Memcmp(
			Consumer.GetRecords()[0].FixIts[0].ReplacementUtf8.GetData(),
			Expected,
			UE_ARRAY_COUNT(Expected)) == 0));
	}

	TEST_METHOD(ParallelFragmentMergeMatchesSingleThreadOrder)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeSnapshot(TEXT("/Angelscript/Game/Ordering.as"), TEXT("abcdefgh"), FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Snapshot);

		auto Populate = [&Manager, FileID](AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine& Diagnostics, bool bParallel)
		{
			auto SubmitFragment = [&Manager, FileID, &Diagnostics](int32 FragmentIndex)
			{
				const FString FragmentKey = FString::Printf(TEXT("fragment-%d"), FragmentIndex);
				AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Diagnostics.CreateFragment(FragmentKey);
				for (int32 LocalIndex = 0; LocalIndex < 2; ++LocalIndex)
				{
					AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Record;
					Record.DiagnosticID = 4300 + static_cast<asUINT>(FragmentIndex);
					Record.Severity = FragmentIndex == 0
						? AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Warning
						: AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error;
					Record.PrimaryRange = Manager.MakeRange(FileID, 2, 4);
					Record.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromSigned(LocalIndex));
					Fragment.Report(MoveTemp(Record));
				}
				Diagnostics.Submit(MoveTemp(Fragment));
			};

			if (bParallel)
			{
				ParallelFor(2, SubmitFragment);
			}
			else
			{
				SubmitFragment(0);
				SubmitFragment(1);
			}
		};

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine SingleThreaded(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine MultiThreaded(Manager);
		Populate(SingleThreaded, false);
		Populate(MultiThreaded, true);
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer SingleConsumer;
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer MultiConsumer;
		SingleThreaded.EmitTo(SingleConsumer);
		MultiThreaded.EmitTo(MultiConsumer);

		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::SerializeStable(SingleConsumer.GetRecords()),
			AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::SerializeStable(MultiConsumer.GetRecords())));
		ASSERT_THAT(AreEqual(4, SingleConsumer.GetRecords().Num()));
		ASSERT_THAT(AreEqual(4301u, SingleConsumer.GetRecords()[0].DiagnosticID));
		ASSERT_THAT(AreEqual(0ll, SingleConsumer.GetRecords()[0].Arguments[0].SignedValue));
		ASSERT_THAT(AreEqual(1ll, SingleConsumer.GetRecords()[1].Arguments[0].SignedValue));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
