#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/Lexer/as_frontend_options.h"
#include "frontend/Lexer/as_preprocessor.h"
#include "frontend/Lexer/as_token.h"
#include "frontend/Lexer/as_tokenizer.h"

#include "Templates/UniquePtr.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

static_assert(!std::is_copy_constructible_v<AS_NAMESPACE_QUALIFIER asCDiagnostic>);
static_assert(!std::is_copy_assignable_v<AS_NAMESPACE_QUALIFIER asCDiagnostic>);

namespace
{
	class FCountingDiagnosticConsumer final : public AS_NAMESPACE_QUALIFIER asIDiagnosticConsumer
	{
	public:
		void HandleDiagnostics(
			TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> OrderedRecords) override
		{
			++CallCount;
			Records.Reset();
			Records.Append(OrderedRecords.GetData(), OrderedRecords.Num());
		}

		int32 CallCount = 0;
		TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records;
	};

	TArray<AS_NAMESPACE_QUALIFIER asCToken> LexAll(AS_NAMESPACE_QUALIFIER asCTokenizer& Tokenizer)
	{
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!Tokenizer.Lex(Token))
			{
				break;
			}
			Tokens.Add(Token);
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		return Tokens;
	}
}

TEST_CLASS_WITH_FLAGS(
	DiagnosticProduction,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TemporaryChainOwnsPayload)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(
				TEXT("/Angelscript/Game/bad.as"),
				TEXT("bad"),
				&FileID);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment =
			Diagnostics.CreateFragment(TEXT("lexer"));
		const AS_NAMESPACE_QUALIFIER asCSourceRange Primary = Manager.MakeRange(FileID, 0, 3);
		const AS_NAMESPACE_QUALIFIER asCSourceRange Related = Manager.MakeRange(FileID, 1, 2);
		FCountingDiagnosticConsumer Consumer;

		FString TokenText(TEXT("token"));
		AS_NAMESPACE_QUALIFIER asSDiagnosticFixIt Fix =
			AS_NAMESPACE_QUALIFIER asSDiagnosticFixIt::FromText(Primary, TEXT("good"));
		Fragment.Diag(Primary, 1001)
			<< FStringView(TokenText)
			<< int64(-7)
			<< true
			<< AS_NAMESPACE_QUALIFIER asSDiagnosticArgument::FromUnsigned(9)
			<< Related
			<< Fix;
		TokenText.Reset();
		Fix = AS_NAMESPACE_QUALIFIER asSDiagnosticFixIt();
		ASSERT_THAT(AreEqual(0, Consumer.CallCount));
		ASSERT_THAT(AreEqual(0u, Diagnostics.GetPendingCount()));
		ASSERT_THAT(IsTrue(Diagnostics.Submit(MoveTemp(Fragment))));
		ASSERT_THAT(AreEqual(0, Consumer.CallCount));
		ASSERT_THAT(AreEqual(1u, Diagnostics.GetPendingCount()));

		Diagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(1, Consumer.CallCount));
		ASSERT_THAT(AreEqual(1, Consumer.Records.Num()));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record = Consumer.Records[0];
		ASSERT_THAT(AreEqual(1001u, Record.DiagnosticID));
		ASSERT_THAT(AreEqual(0u, Record.PrimaryRange.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(3u, Record.PrimaryRange.End.ByteOffset));
		ASSERT_THAT(AreEqual(4, Record.Arguments.Num()));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Text,
			Record.Arguments[0].Kind));
		ASSERT_THAT(AreEqual(FString(TEXT("token")), Record.Arguments[0].TextValue));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Signed,
			Record.Arguments[1].Kind));
		ASSERT_THAT(AreEqual(-7ll, Record.Arguments[1].SignedValue));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Boolean,
			Record.Arguments[2].Kind));
		ASSERT_THAT(IsTrue(Record.Arguments[2].BooleanValue));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEDiagnosticArgumentKind::Unsigned,
			Record.Arguments[3].Kind));
		ASSERT_THAT(AreEqual(9ull, Record.Arguments[3].UnsignedValue));
		ASSERT_THAT(AreEqual(1, Record.RelatedRanges.Num()));
		ASSERT_THAT(AreEqual(1u, Record.RelatedRanges[0].Begin.ByteOffset));
		ASSERT_THAT(AreEqual(2u, Record.RelatedRanges[0].End.ByteOffset));
		ASSERT_THAT(AreEqual(1, Record.FixIts.Num()));
		const uint8 ExpectedFix[] = {0x67, 0x6F, 0x6F, 0x64};
		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(ExpectedFix), Record.FixIts[0].ReplacementUtf8.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(
			Record.FixIts[0].ReplacementUtf8.GetData(),
			ExpectedFix,
			UE_ARRAY_COUNT(ExpectedFix)) == 0));
	}

	TEST_METHOD(SingleReportAndExplicitSubmission)
	{
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
			AngelscriptNativeEngineTest::MakeSourceManager(
				TEXT("/Angelscript/Game/owned.as"),
				TEXT("bad"),
				&FileID);
		const AS_NAMESPACE_QUALIFIER asCSourceRange Range = Manager.MakeRange(FileID, 0, 3);

		{
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
			AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment =
				Diagnostics.CreateFragment(TEXT("owned"));
			TUniquePtr<AS_NAMESPACE_QUALIFIER asCDiagnostic> Live =
				MakeUnique<AS_NAMESPACE_QUALIFIER asCDiagnostic>(Fragment.Diag(Range, 1001));
			TUniquePtr<AS_NAMESPACE_QUALIFIER asCDiagnostic> Owner =
				MakeUnique<AS_NAMESPACE_QUALIFIER asCDiagnostic>(MoveTemp(*Live));
			Live.Reset();
			ASSERT_THAT(AreEqual(0u, Diagnostics.GetPendingCount()));
			Owner.Reset();
			ASSERT_THAT(AreEqual(0u, Diagnostics.GetPendingCount()));
			ASSERT_THAT(IsTrue(Diagnostics.Submit(MoveTemp(Fragment))));
			ASSERT_THAT(AreEqual(1u, Diagnostics.GetPendingCount()));
		}

		{
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
			AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment =
				Diagnostics.CreateFragment(TEXT("nostream"));
			Fragment.Diag(Range, 1001);
			ASSERT_THAT(AreEqual(0u, Diagnostics.GetPendingCount()));
			ASSERT_THAT(IsTrue(Diagnostics.Submit(MoveTemp(Fragment))));
			ASSERT_THAT(AreEqual(1u, Diagnostics.GetPendingCount()));
		}

		{
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
			{
				AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Abandoned =
					Diagnostics.CreateFragment(TEXT("abandoned"));
				Abandoned.Diag(Range, 1001);
			}
			ASSERT_THAT(AreEqual(0u, Diagnostics.GetPendingCount()));
		}

		{
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
			AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
			AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
				Manager,
				Manager.MakeRange(FileID, 0, 3),
				AS_NAMESPACE_QUALIFIER asSLexOptions(),
				Identifiers,
				Diagnostics);
			const TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens = LexAll(Tokenizer);
			ASSERT_THAT(IsTrue(Tokenizer.FlushDiagnostics()));
			const asUINT AfterFlush = Diagnostics.GetPendingCount();
			{
				AS_NAMESPACE_QUALIFIER asCDiagnostic Misuse =
					Tokenizer.Diag(Range, 1001);
				Misuse << FStringView(TEXT("token"));
			}
			ASSERT_THAT(AreEqual(AfterFlush, Diagnostics.GetPendingCount()));
			ASSERT_THAT(IsFalse(Tokenizer.FlushDiagnostics()));
			ASSERT_THAT(AreEqual(AfterFlush, Diagnostics.GetPendingCount()));

			asUINT AfterHeldFlush = 0;
			{
				AS_NAMESPACE_QUALIFIER asCTokenizer Second(
					Manager,
					Manager.MakeRange(FileID, 0, 3),
					AS_NAMESPACE_QUALIFIER asSLexOptions(),
					Identifiers,
					Diagnostics);
				AS_NAMESPACE_QUALIFIER asCDiagnostic Held = Second.Diag(Range, 1001);
				ASSERT_THAT(IsTrue(Second.FlushDiagnostics()));
				AfterHeldFlush = Diagnostics.GetPendingCount();
			}
			ASSERT_THAT(AreEqual(AfterHeldFlush, Diagnostics.GetPendingCount()));
		}
	}

	TEST_METHOD(LocalErrorSurvivesFlush)
	{
		const uint8 UnterminatedBytes[] = {0x22, 0x61, 0x62, 0x63};
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SharedSources;
		SharedSources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(
			TEXT("A.as"),
			MakeArrayView(UnterminatedBytes)));
		SharedSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
			TEXT("B.as"),
			TEXT("int B;")));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(SharedSources);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileA(1);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileB(2);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;

		AS_NAMESPACE_QUALIFIER asCTokenizer TokenizerA(
			Manager,
			Manager.MakeRange(FileA, 0, 4),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			Identifiers,
			Diagnostics);
		const TArray<AS_NAMESPACE_QUALIFIER asCToken> TokensA = LexAll(TokenizerA);
		ASSERT_THAT(AreEqual(2, TokensA.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral, TokensA[0].Kind));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(
			TokensA[0].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated)));
		ASSERT_THAT(AreEqual(0u, TokensA[0].Range.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(4u, TokensA[0].Range.End.ByteOffset));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, TokensA[1].Kind));
		ASSERT_THAT(AreEqual(4u, TokensA[1].Range.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(4u, TokensA[1].Range.End.ByteOffset));
		ASSERT_THAT(IsTrue(TokenizerA.HasErrors()));
		ASSERT_THAT(IsTrue(TokenizerA.FlushDiagnostics()));
		ASSERT_THAT(IsTrue(TokenizerA.HasErrors()));

		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords().Num()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnterminatedString),
			Consumer.GetRecords()[0].DiagnosticID));
		ASSERT_THAT(AreEqual(0u, Consumer.GetRecords()[0].PrimaryRange.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(4u, Consumer.GetRecords()[0].PrimaryRange.End.ByteOffset));

		AS_NAMESPACE_QUALIFIER asCTokenizer TokenizerB(
			Manager,
			Manager.MakeRange(FileB, 0, 6),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			Identifiers,
			Diagnostics);
		const TArray<AS_NAMESPACE_QUALIFIER asCToken> TokensB = LexAll(TokenizerB);
		ASSERT_THAT(IsTrue(TokensB.Num() > 0));
		ASSERT_THAT(IsFalse(TokenizerB.HasErrors()));
		ASSERT_THAT(IsTrue(TokenizerB.FlushDiagnostics()));
		ASSERT_THAT(IsFalse(TokenizerB.HasErrors()));
	}

	TEST_METHOD(PPAndMergeCompatibility)
	{
		{
			AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
			const AS_NAMESPACE_QUALIFIER asCSourceManager Manager =
				AngelscriptNativeEngineTest::MakeSourceManager(
					TEXT("/Angelscript/Game/Include.as"),
					TEXT("#include \"X.as\""),
					&FileID);
			AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
			AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
				Manager,
				Manager.MakeRange(FileID, 0, 15),
				AS_NAMESPACE_QUALIFIER asSLexOptions(
					AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
					AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
				Identifiers,
				Diagnostics);
			const TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens = LexAll(Tokenizer);
			Tokenizer.FlushDiagnostics();
			FCountingDiagnosticConsumer Consumer;
			const AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration;
			AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor(Manager, Configuration, Diagnostics);
			Preprocessor.Process(Tokens);
			ASSERT_THAT(AreEqual(0, Consumer.CallCount));
			Diagnostics.EmitTo(Consumer);
			ASSERT_THAT(AreEqual(1, Consumer.CallCount));
			ASSERT_THAT(AreEqual(1, Consumer.Records.Num()));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::UnsupportedInclude),
				Consumer.Records[0].DiagnosticID));
			ASSERT_THAT(AreEqual(0u, Consumer.Records[0].PrimaryRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(15u, Consumer.Records[0].PrimaryRange.End.ByteOffset));
		}

		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SharedSources;
		SharedSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
			TEXT("/Angelscript/Game/A.as"),
			TEXT("aaa")));
		SharedSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
			TEXT("/Angelscript/Game/B.as"),
			TEXT("bbb")));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(SharedSources);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileA(1);
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileB(2);

		auto SubmitPair = [&Manager, FileA, FileB](
			AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine& Diagnostics,
			bool bAFirst)
		{
			auto SubmitOne = [&Manager, &Diagnostics](
				AS_NAMESPACE_QUALIFIER asCSourceFileID FileID,
				FStringView FragmentKey,
				asUINT DiagnosticID)
			{
				AS_NAMESPACE_QUALIFIER asCDiagnosticFragment Fragment =
					Diagnostics.CreateFragment(FragmentKey);
				Fragment.Diag(Manager.MakeRange(FileID, 0, 3), DiagnosticID);
				Diagnostics.Submit(MoveTemp(Fragment));
			};
			if (bAFirst)
			{
				SubmitOne(FileA, TEXT("file-A"), 1001);
				SubmitOne(FileB, TEXT("file-B"), 1004);
			}
			else
			{
				SubmitOne(FileB, TEXT("file-B"), 1004);
				SubmitOne(FileA, TEXT("file-A"), 1001);
			}
		};

		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine OrderAB(Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine OrderBA(Manager);
		SubmitPair(OrderAB, true);
		SubmitPair(OrderBA, false);
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer ConsumerAB;
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer ConsumerBA;
		OrderAB.EmitTo(ConsumerAB);
		OrderBA.EmitTo(ConsumerBA);
		ASSERT_THAT(AreEqual(2, ConsumerAB.GetRecords().Num()));
		ASSERT_THAT(AreEqual(2, ConsumerBA.GetRecords().Num()));
		ASSERT_THAT(AreEqual(1001u, ConsumerAB.GetRecords()[0].DiagnosticID));
		ASSERT_THAT(AreEqual(1004u, ConsumerAB.GetRecords()[1].DiagnosticID));
		ASSERT_THAT(AreEqual(0u, ConsumerAB.GetRecords()[0].PrimaryRange.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(3u, ConsumerAB.GetRecords()[0].PrimaryRange.End.ByteOffset));
		ASSERT_THAT(AreEqual(0u, ConsumerAB.GetRecords()[1].PrimaryRange.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(3u, ConsumerAB.GetRecords()[1].PrimaryRange.End.ByteOffset));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::SerializeStable(ConsumerAB.GetRecords()),
			AS_NAMESPACE_QUALIFIER asCDiagnosticRenderer::SerializeStable(ConsumerBA.GetRecords())));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
