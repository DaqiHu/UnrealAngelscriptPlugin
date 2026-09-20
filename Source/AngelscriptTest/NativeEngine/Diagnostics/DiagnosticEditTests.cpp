#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_source_edit.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsEdit,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AtomicCorrection)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(
				TEXT("A.as"),
				TEXT("int Cout=1"),
				&FileID);
		uint64 Revision = 0;
		ASSERT_THAT(IsTrue(Manager.TryGetContentRevision(FileID, Revision)));
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(TEXT("fix"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Group;
		Group.Primary.DiagnosticID = 4003;
		Group.Primary.PrimaryRange = Manager.MakeRange(FileID, 4, 8);
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Fix;
		Fix.Title = TEXT("Count;");
		AS_NAMESPACE_QUALIFIER asSSourceEdit Rename;
		Rename.LogicalSourceKey = TEXT("A.as");
		Rename.ExpectedRevision = Revision;
		Rename.Range = Manager.MakeRange(FileID, 4, 8);
		Rename.ExpectedOriginalUtf8 = { 'C', 'o', 'u', 't' };
		Rename.ReplacementUtf8 = { 'C', 'o', 'u', 'n', 't' };
		AS_NAMESPACE_QUALIFIER asSSourceEdit Semi;
		Semi.LogicalSourceKey = TEXT("A.as");
		Semi.ExpectedRevision = Revision;
		Semi.Range = Manager.MakeRange(FileID, 10, 10);
		Semi.ReplacementUtf8 = { ';' };
		Fix.SourceEdits.Add(Rename);
		Fix.SourceEdits.Add(Semi);
		Group.Fixes.Add(Fix);
		ASSERT_THAT(IsTrue(Fragment.ReportGroup(MoveTemp(Group))));
		ASSERT_THAT(IsTrue(Engine.Submit(MoveTemp(Fragment))));

		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Engine.CaptureResult();
		TConstArrayView<uint8> Original;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(FileID, Original)));
		const TArray<uint8> OriginalCopy(Original.GetData(), Original.Num());
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		AS_NAMESPACE_QUALIFIER asCSourceEditApplier Applier;
		ASSERT_THAT(IsTrue(Applier.Apply(*Result, Result->GetGroups()[0].Fixes[0], Snapshot)));
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		TConstArrayView<uint8> Next;
		ASSERT_THAT(IsTrue(Snapshot->GetManager().TryGetBytes(
			AS_NAMESPACE_QUALIFIER asCSourceFileID(1),
			Next)));
		const uint8 Expected[] = {'i', 'n', 't', ' ', 'C', 'o', 'u', 'n', 't', '=', '1', ';'};
		ASSERT_THAT(AreEqual(static_cast<int32>(UE_ARRAY_COUNT(Expected)), Next.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Next.GetData(), Expected, UE_ARRAY_COUNT(Expected)) == 0));
		TConstArrayView<uint8> Unchanged;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(FileID, Unchanged)));
		ASSERT_THAT(AreEqual(OriginalCopy.Num(), Unchanged.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Unchanged.GetData(), OriginalCopy.GetData(), OriginalCopy.Num()) == 0));
		uint64 After = 0;
		ASSERT_THAT(IsTrue(Manager.TryGetContentRevision(FileID, After)));
		ASSERT_THAT(AreEqual(Revision, After));
	}

	TEST_METHOD(AllOrNothingPreconditions)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int A;")));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Sources);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileA(1);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileB(2);
		uint64 RevA = 0;
		uint64 RevB = 0;
		ASSERT_THAT(IsTrue(Manager.TryGetContentRevision(FileA, RevA)));
		ASSERT_THAT(IsTrue(Manager.TryGetContentRevision(FileB, RevB)));
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(TEXT("fix"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Group;
		Group.Primary.DiagnosticID = 4003;
		Group.Primary.PrimaryRange = Manager.MakeRange(FileA, 0, 3);
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Stale;
		AS_NAMESPACE_QUALIFIER asSSourceEdit First;
		First.LogicalSourceKey = TEXT("A.as");
		First.ExpectedRevision = RevA;
		First.Range = Manager.MakeRange(FileA, 4, 5);
		First.ExpectedOriginalUtf8 = { 'A' };
		First.ReplacementUtf8 = { 'X' };
		AS_NAMESPACE_QUALIFIER asSSourceEdit Second;
		Second.LogicalSourceKey = TEXT("B.as");
		Second.ExpectedRevision = RevB + 1;
		Second.Range = Manager.MakeRange(FileB, 4, 5);
		Second.ExpectedOriginalUtf8 = { 'B' };
		Second.ReplacementUtf8 = { 'Y' };
		Stale.SourceEdits.Add(First);
		Stale.SourceEdits.Add(Second);
		Group.Fixes.Add(Stale);
		ASSERT_THAT(IsTrue(Fragment.ReportGroup(MoveTemp(Group))));
		ASSERT_THAT(IsTrue(Engine.Submit(MoveTemp(Fragment))));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Engine.CaptureResult();
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		AS_NAMESPACE_QUALIFIER asCSourceEditApplier Applier;
		ASSERT_THAT(IsFalse(Applier.Apply(*Result, Result->GetGroups()[0].Fixes[0], Snapshot)));
		ASSERT_THAT(IsFalse(Snapshot.IsValid()));
		TConstArrayView<uint8> StillA;
		ASSERT_THAT(IsTrue(Manager.TryGetBytes(FileA, StillA)));
		ASSERT_THAT(AreEqual(static_cast<uint8>('A'), StillA[4]));

		AS_NAMESPACE_QUALIFIER asCSourceFileID ForeignID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Foreign =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("A.as"), TEXT("int A;"), &ForeignID);
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative ForeignFix;
		AS_NAMESPACE_QUALIFIER asSSourceEdit ForeignEdit;
		ForeignEdit.LogicalSourceKey = TEXT("A.as");
		ForeignEdit.ExpectedRevision = RevA;
		ForeignEdit.Range = Foreign.MakeRange(ForeignID, 4, 5);
		ForeignEdit.ExpectedOriginalUtf8 = { 'A' };
		ForeignEdit.ReplacementUtf8 = { 'Z' };
		ForeignFix.SourceEdits.Add(ForeignEdit);
		ASSERT_THAT(IsFalse(Applier.Apply(*Result, ForeignFix, Snapshot)));

		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Overlap;
		AS_NAMESPACE_QUALIFIER asSSourceEdit Left = First;
		AS_NAMESPACE_QUALIFIER asSSourceEdit Right = First;
		Right.Range = Manager.MakeRange(FileA, 4, 5);
		Overlap.SourceEdits.Add(Left);
		Overlap.SourceEdits.Add(Right);
		ASSERT_THAT(IsFalse(Applier.Apply(*Result, Overlap, Snapshot)));
	}
};

#endif
