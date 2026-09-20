#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"

#include "as_builder.h"
#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Lexer/as_tokenizer.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsCore,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OwnedNonlocatedGroup)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		AS_NAMESPACE_QUALIFIER asCSourceFileID ForeignFileID;
		AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("A.as"), TEXT("int A;"), &FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceManager Foreign =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("A.as"), TEXT("int A;"), &ForeignFileID);

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Engine(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(TEXT("driver"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Group;
		Group.Primary.DiagnosticID = static_cast<asUINT>(
			AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InputAdmission);
		Group.Primary.bHasLocation = false;
		Group.Primary.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("empty-source-batch")));
		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Note;
		Note.DiagnosticID = static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::AttachedNote);
		Note.bHasLocation = false;
		Note.Arguments.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("prepare sources")));
		Group.Notes.Add(Note);
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Fix;
		Fix.Title = TEXT("add-file");
		Group.Fixes.Add(Fix);
		ASSERT_THAT(IsTrue(Fragment.ReportGroup(Group)));

		AS_NAMESPACE_QUALIFIER asSDiagnosticGroup ForeignGroup;
		ForeignGroup.Primary.DiagnosticID = static_cast<asUINT>(
			AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::InvalidUtf8);
		ForeignGroup.Primary.PrimaryRange = Foreign.MakeRange(ForeignFileID, 0, 1);
		ASSERT_THAT(IsFalse(Fragment.ReportGroup(ForeignGroup)));
		ASSERT_THAT(IsTrue(Engine.Submit(MoveTemp(Fragment))));

		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Engine.CaptureResult();
		Manager = AS_NAMESPACE_QUALIFIER asCSourceManager();
		ASSERT_THAT(AreEqual(1, Result->GetGroups().Num()));
		ASSERT_THAT(IsFalse(Result->GetGroups()[0].Primary.bHasLocation));
		ASSERT_THAT(AreEqual(1, Result->GetGroups()[0].Notes.Num()));
		ASSERT_THAT(AreEqual(1, Result->GetGroups()[0].Fixes.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("empty-source-batch")),
			Result->GetGroups()[0].Primary.Arguments[0].TextValue));
		ASSERT_THAT(IsTrue(Result->HasLanguageFailure()));
	}

	TEST_METHOD(DeterministicPolicy)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileA;
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileB;
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int A;")));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Sources);
		FileA = AS_NAMESPACE_QUALIFIER asCSourceFileID(1);
		FileB = AS_NAMESPACE_QUALIFIER asCSourceFileID(2);

		auto SubmitPair = [&](AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine& Engine, bool bBFirst)
		{
			auto SubmitOne = [&](AS_NAMESPACE_QUALIFIER asCSourceFileID File, FStringView Key, asUINT ID)
			{
				AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(Key);
				Fragment.Diag(Manager.MakeRange(File, 0, 5), ID);
				Engine.Submit(MoveTemp(Fragment));
			};
			if (bBFirst)
			{
				SubmitOne(FileB, TEXT("file-B"), 1004);
				SubmitOne(FileA, TEXT("file-A"), 1001);
			}
			else
			{
				SubmitOne(FileA, TEXT("file-A"), 1001);
				SubmitOne(FileB, TEXT("file-B"), 1004);
			}
		};

		AS_NAMESPACE_QUALIFIER asSDiagnosticOptions Options;
		Options.ErrorReportLimit = 1;
		Options.bSuppressDisplay = true;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine OrderBA(Manager, Options);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine OrderAB(Manager, Options);
		SubmitPair(OrderBA, true);
		SubmitPair(OrderAB, false);

		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> ResultBA =
			OrderBA.CaptureResult();
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> ResultAB =
			OrderAB.CaptureResult();
		ASSERT_THAT(AreEqual(1, ResultBA->GetGroups().Num()));
		ASSERT_THAT(AreEqual(1, ResultAB->GetGroups().Num()));
		ASSERT_THAT(AreEqual(1001u, ResultBA->GetGroups()[0].Primary.DiagnosticID));
		ASSERT_THAT(AreEqual(1001u, ResultAB->GetGroups()[0].Primary.DiagnosticID));
		ASSERT_THAT(AreEqual(0, ResultBA->GetVisibleGroups().Num()));
		ASSERT_THAT(IsTrue(ResultBA->IsTruncated()));
		ASSERT_THAT(IsTrue(ResultAB->IsTruncated()));
		ASSERT_THAT(IsTrue(ResultBA->HasLanguageFailure()));
		ASSERT_THAT(IsTrue(ResultAB->HasLanguageFailure()));

		AS_NAMESPACE_QUALIFIER asSDiagnosticOptions WarnOptions;
		WarnOptions.bWarningsAsErrors = true;
		WarnOptions.PerDiagnosticEnabled.Add(4201, false);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine WarnEngine(Manager, WarnOptions);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment WarnFragment = WarnEngine.CreateFragment(TEXT("warn"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Warning;
		Warning.DiagnosticID = 4201;
		Warning.Severity = AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Warning;
		Warning.PrimaryRange = Manager.MakeRange(FileA, 0, 5);
		ASSERT_THAT(IsTrue(WarnFragment.Report(MoveTemp(Warning))));
		ASSERT_THAT(IsTrue(WarnEngine.Submit(MoveTemp(WarnFragment))));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> WarnResult =
			WarnEngine.CaptureResult();
		ASSERT_THAT(AreEqual(0, WarnResult->GetGroups().Num()));
		ASSERT_THAT(IsFalse(WarnResult->HasLanguageFailure()));
		ASSERT_THAT(IsFalse(WarnResult->HasWarningPolicyFailure()));
	}

	TEST_METHOD(HiddenLexicalFailureStillGatesOwnFile)
	{
		const uint8 Malformed[] = {0xC0, 0xAF};
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(
			TEXT("A.as"),
			MakeArrayView(Malformed)));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("diagnostics.core");
		Options.Diagnostics.bSuppressDisplay = true;
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		ASSERT_THAT(AreEqual(1, Builder.GetPreprocessedSources().Num()));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Builder.CaptureDiagnosticResult();
		ASSERT_THAT(AreEqual(0, Result->GetVisibleGroups().Num()));
		ASSERT_THAT(IsTrue(Result->HasLanguageFailure()));
		ASSERT_THAT(IsTrue(Result->GetGroups().Num() >= 1));

		AS_NAMESPACE_QUALIFIER asSBuilderOptions WarnOptions;
		WarnOptions.StableScope = TEXT("diagnostics.core.warn");
		WarnOptions.Diagnostics.bWarningsAsErrors = true;
		WarnOptions.Diagnostics.PerDiagnosticEnabled.Add(4201, false);
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Clean =
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("C.as"), TEXT("int C;"));
		AS_NAMESPACE_QUALIFIER asCBuilder CleanBuilder(Clean, WarnOptions);
		ASSERT_THAT(IsTrue(CleanBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		ASSERT_THAT(IsFalse(CleanBuilder.CaptureDiagnosticResult()->HasLanguageFailure()));
	}

	TEST_METHOD(AcceptedBoundaries)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int A;")));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Sources);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileA(1);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileB(2);

		auto SubmitCross = [&](AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine& Engine, bool bReverse)
		{
			auto Submit = [&](FStringView Key)
			{
				AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Engine.CreateFragment(Key);
				AS_NAMESPACE_QUALIFIER asSDiagnosticGroup Group;
				Group.Primary.DiagnosticID = 1001;
				Group.Primary.PrimaryRange = Manager.MakeRange(FileA, 0, 3);
				AS_NAMESPACE_QUALIFIER asSDiagnosticRecord Note;
				Note.DiagnosticID = 6001;
				Note.PrimaryRange = Manager.MakeRange(FileB, 0, 3);
				Group.Notes.Add(Note);
				AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative Fix;
				Fix.Title = TEXT("rewrite");
				Fix.Edits.Add(AS_NAMESPACE_QUALIFIER asSDiagnosticFixIt::FromText(
					Manager.MakeRange(FileA, 0, 3), TEXT("int")));
				Group.Fixes.Add(Fix);
				Fragment.ReportGroup(MoveTemp(Group));
				Engine.Submit(MoveTemp(Fragment));
			};
			if (bReverse)
			{
				Submit(TEXT("later"));
			}
			else
			{
				Submit(TEXT("earlier"));
			}
		};

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine First(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Second(Manager);
		SubmitCross(First, false);
		SubmitCross(Second, true);
		ASSERT_THAT(AreEqual(1, First.CaptureResult()->GetGroups().Num()));
		ASSERT_THAT(AreEqual(1, Second.CaptureResult()->GetGroups().Num()));
		ASSERT_THAT(AreEqual(1, First.CaptureResult()->GetGroups()[0].Notes.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("B.as")), First.CaptureResult()->GetGroups()[0].Notes[0].StableSourceKey));

		for (const int32 Workers : {1, 4})
		{
			AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
			Options.StableScope = TEXT("diagnostics.core.workers");
			Options.WorkerCount = Workers;
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
			ASSERT_THAT(AreEqual(2, Builder.GetLexedSources().Num()));
		}

		AS_NAMESPACE_QUALIFIER asCSourceManager Released =
			AngelscriptNativeEngineTest::MakeSourceManager(TEXT("A.as"), TEXT("int A;"));
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Owned(Released);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment = Owned.CreateFragment(TEXT("keep"));
		Fragment.Diag(Released.MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 0, 3), 1001)
			<< AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromText(TEXT("token"));
		Owned.Submit(MoveTemp(Fragment));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Held =
			Owned.CaptureResult();
		Released = AS_NAMESPACE_QUALIFIER asCSourceManager();
		TConstArrayView<uint8> Bytes;
		ASSERT_THAT(IsTrue(Held->GetSources().TryGetBytes(
			Held->GetGroups()[0].Primary.PrimaryRange,
			Bytes)));
		ASSERT_THAT(AreEqual(3, Bytes.Num()));
	}
};

#endif
