#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "../Tooling/NativeToolingTestSupport.h"

#include "as_builder.h"
#include "frontend/Basic/as_source_edit.h"
#include "frontend/Compile/as_language_service.h"
#include "frontend/Sema/as_tooling_session.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsToolingIntegration,
	"Angelscript.UnitTest.NativeEngine.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	static const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* FindId(
		TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticGroup> Groups,
		asUINT Id)
	{
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Groups)
		{
			if (Group.Primary.DiagnosticID == Id)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	static int32 CountId(
		TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticGroup> Groups,
		asUINT Id)
	{
		int32 Count = 0;
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Groups)
		{
			if (Group.Primary.DiagnosticID == Id)
			{
				++Count;
			}
		}
		return Count;
	}

	static TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SourcesFromSnapshot(
		const AS_NAMESPACE_QUALIFIER asCSourceSnapshot& Snapshot)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		const AS_NAMESPACE_QUALIFIER asCSourceManager& Manager = Snapshot.GetManager();
		for (asUINT Index = 1; Index <= Manager.GetFileCount(); ++Index)
		{
			const AS_NAMESPACE_QUALIFIER asCSourceFileID File(Index);
			FStringView Key;
			TConstArrayView<uint8> Bytes;
			if (Manager.TryGetLogicalSourceKey(File, Key) && Manager.TryGetBytes(File, Bytes))
			{
				Sources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(Key, Bytes));
			}
		}
		return Sources;
	}

	TEST_METHOD(RealCompileFixReanalysis)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("integration.fix");
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("Typo.as"),
				TEXT("int F(){ int Count=1; return Cout; }")),
			Options);
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Original =
			Builder.GetCompileOutput()->GetDiagnosticResult();
		std::shared_ptr<AS_NAMESPACE_QUALIFIER asCLanguageService> Service =
			AS_NAMESPACE_QUALIFIER asCLanguageService::Create();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->AttachCompilation(Original))));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindId(
			Service->GetGroups(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference));
		ASSERT_THAT(IsNotNull(Group));
		FString Text;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->Format(*Group, Text))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("Cout"))));
		ASSERT_THAT(AreEqual(1, Group->Fixes.Num()));
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->ApplyFix(Group->Fixes[0], Snapshot))));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticFixAlternative OldFix = Group->Fixes[0];
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));

		AS_NAMESPACE_QUALIFIER asCBuilder FreshBuilder(SourcesFromSnapshot(*Snapshot), Options);
		ASSERT_THAT(IsTrue(FreshBuilder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Fresh =
			FreshBuilder.GetCompileOutput()->GetDiagnosticResult();
		ASSERT_THAT(AreEqual(
			0,
			CountId(
				Fresh->GetGroups(),
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference))));
		ASSERT_THAT(IsNotNull(FindId(
			Original->GetGroups(),
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference))));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::Succeeded),
			static_cast<int32>(Service->AttachCompilation(Fresh))));
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Rejected;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asELanguageServiceStatus::OwnerMismatch),
			static_cast<int32>(Service->ApplyFix(OldFix, Rejected))));
		ASSERT_THAT(IsFalse(Rejected.IsValid()));
		const AS_NAMESPACE_QUALIFIER asCSourceEditApplier Applier;
		TSharedPtr<const AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Stale;
		ASSERT_THAT(IsFalse(Applier.Apply(*Fresh, OldFix, Stale)));
	}

	TEST_METHOD(PartialSourceAndDeterminism)
	{
		const FString Source = TEXT("int F(){ return Missing; } int G(){ int Ready=2; return Ready; }");
		auto AnalyzeWith = [&](int32 Workers, bool bReverse)
		{
			TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Types =
				MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
			TSharedPtr<asCDefinitions, ESPMode::ThreadSafe> Host =
				AngelscriptNativeToolingTest::MakeHostType(Types, TEXT("HostValue"));
			TArray<TPair<FString, FString>> Files;
			if (bReverse)
			{
				Files.Add({TEXT("Other.as"), TEXT("int H(){ return 3; }")});
				Files.Add({TEXT("Partial.as"), Source});
			}
			else
			{
				Files.Add({TEXT("Partial.as"), Source});
				Files.Add({TEXT("Other.as"), TEXT("int H(){ return 3; }")});
			}
			TSharedRef<AS_NAMESPACE_QUALIFIER asSAnalysisInputs, ESPMode::ThreadSafe> Inputs =
				AngelscriptNativeToolingTest::MakeInputs(Files, Types, Host);
			Inputs->Options.WorkerCount = Workers;
			Inputs->Options.LexBatchSize = 4;
			AS_NAMESPACE_QUALIFIER asCToolingSession Session(Inputs);
			return Session.Analyze({});
		};

		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse One = AnalyzeWith(1, false);
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse Four = AnalyzeWith(4, true);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Partial),
			static_cast<int32>(One.Status)));
		ASSERT_THAT(AreEqual(One.Status, Four.Status));
		ASSERT_THAT(IsTrue(One.Result.IsValid() && Four.Result.IsValid()));
		ASSERT_THAT(IsFalse(One.Result->GetSession()->IsPublishable()));
		ASSERT_THAT(AreEqual(
			One.Result->GetSession()->SerializeStableDiagnostics(),
			Four.Result->GetSession()->SerializeStableDiagnostics()));

		AS_NAMESPACE_QUALIFIER asSQueryPosition Ready;
		Ready.LogicalSourceKey = TEXT("Partial.as");
		Ready.ByteOffset = static_cast<asUINT>(Source.Find(TEXT("return Ready")) + 7);
		AS_NAMESPACE_QUALIFIER asCToolingSession Query(
			AngelscriptNativeToolingTest::MakeInputs({{TEXT("Partial.as"), Source}}));
		const AS_NAMESPACE_QUALIFIER asSAnalysisResponse ForQuery = Query.Analyze({});
		const AS_NAMESPACE_QUALIFIER asSHoverResult Hover = Query.GetHover(*ForQuery.Result, Ready, {});
		ASSERT_THAT(AreEqual(
			static_cast<int32>(AS_NAMESPACE_QUALIFIER asEAnalysisStatus::Success),
			static_cast<int32>(Hover.Status)));
		ASSERT_THAT(IsTrue(Hover.Display.Contains(TEXT("Ready"))));
		AS_NAMESPACE_QUALIFIER asSQueryPosition CompleteAt;
		CompleteAt.LogicalSourceKey = TEXT("Partial.as");
		CompleteAt.ByteOffset = static_cast<asUINT>(Source.Find(TEXT("int Ready=2; ")) + 13);
		const AS_NAMESPACE_QUALIFIER asSCompletionResult Completion = Query.Complete(CompleteAt, {});
		bool bHasReady = false;
		for (const AS_NAMESPACE_QUALIFIER asSCompletionItem& Item : Completion.Items)
		{
			bHasReady = bHasReady || Item.Name == TEXT("Ready");
		}
		ASSERT_THAT(IsTrue(bHasReady));
		ASSERT_THAT(AreEqual(
			ForQuery.Result->GetSession()->SerializeStableBodyProjection(),
			Query.Analyze({}).Result->GetSession()->SerializeStableBodyProjection()));
	}

	TEST_METHOD(JoinedEarlyPhaseFailureAndRetainedToolingData)
	{
		const uint8 Unterminated[] = {0x22, 0x61, 0x62, 0x63};
		const int32 Settings[][2] = {{1, 1}, {1, 4}, {4, 1}, {4, 4}};
		FString Canonical;
		for (const int32* Setting : Settings)
		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(
				TEXT("A.as"),
				MakeArrayView(Unterminated)));
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
				TEXT("B.as"),
				TEXT("int B(){ return 2; }")));
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
				TEXT("C.as"),
				TEXT("#if defined(\n#endif\n")));
			AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
			Options.StableScope = TEXT("integration.early");
			Options.WorkerCount = Setting[0];
			Options.LexBatchSize = Setting[1];
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
			const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Held =
				Builder.CaptureDiagnosticResult();
			const int32 UnterminatedCount = CountId(
				Held->GetGroups(),
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnterminatedString));
			const int32 MalformedCount = CountId(
				Held->GetGroups(),
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::MalformedCondition));
			ASSERT_THAT(IsTrue(UnterminatedCount >= 1));
			ASSERT_THAT(IsTrue(MalformedCount >= 1));
			bool bB = false;
			bool bA = false;
			bool bCValid = false;
			const AS_NAMESPACE_QUALIFIER asCSourceManager* Manager = Builder.GetSourceManager();
			ASSERT_THAT(IsNotNull(Manager));
			for (const auto& Processed : Builder.GetPreprocessedSources())
			{
				FString Key;
				for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Processed.RawTokens)
				{
					FStringView Found;
					if (Manager->TryGetLogicalSourceKey(Token.Range.Begin.FileID, Found))
					{
						Key = Found;
						break;
					}
				}
				if (Key == TEXT("B.as") && Processed.IsValid())
				{
					bB = true;
				}
				if (Key == TEXT("A.as"))
				{
					bA = true;
				}
				if (Key == TEXT("C.as") && Processed.IsValid())
				{
					bCValid = true;
				}
			}
			ASSERT_THAT(IsTrue(bB));
			ASSERT_THAT(IsFalse(bA));
			ASSERT_THAT(IsFalse(bCValid));
			if (Builder.GetCompilationSession())
			{
				ASSERT_THAT(IsFalse(Builder.GetCompilationSession()->IsPublishable()));
			}
			const int32 AfterFirst = Held->GetGroups().Num();
			ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
			ASSERT_THAT(AreEqual(AfterFirst, Builder.CaptureDiagnosticResult()->GetGroups().Num()));
			const FString Fingerprint = FString::Printf(
				TEXT("%d:%d:%d"),
				UnterminatedCount,
				MalformedCount,
				AfterFirst);
			if (Canonical.IsEmpty())
			{
				Canonical = Fingerprint;
			}
			else
			{
				ASSERT_THAT(AreEqual(Canonical, Fingerprint));
			}
			ASSERT_THAT(IsTrue(Held->GetGroups().Num() > 0));
		}
	}
};

#endif
