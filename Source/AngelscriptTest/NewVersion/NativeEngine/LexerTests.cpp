#include "CQTest.h"
#include "NativeEngineTestSupport.h"

#include "frontend/as_character_stream.h"
#include "frontend/as_diagnostics.h"
#include "frontend/as_frontend_options.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_source_manager.h"
#include "frontend/as_token.h"
#include "frontend/as_tokenizer.h"

#include "Async/ParallelFor.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FLexerSource
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		asUINT ByteCount = 0;

		explicit FLexerSource(FStringView Text)
			: Snapshot(MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>())
		{
			const AngelscriptNativeEngineTest::FSourceInput Input =
				AngelscriptNativeEngineTest::FSourceInput::FromText(
					TEXT("/Angelscript/Game/Lexer.as"), Text);
			FileID = Snapshot->AddFile(Input.GetLogicalPath(), Input.GetUtf8Bytes());
			ByteCount = static_cast<asUINT>(Input.GetUtf8Bytes().Num());
			Snapshot->Freeze();
		}

		explicit FLexerSource(TConstArrayView<uint8> Bytes)
			: Snapshot(MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>())
		{
			FileID = Snapshot->AddFile(TEXT("/Angelscript/Game/LexerBytes.as"), Bytes);
			ByteCount = static_cast<asUINT>(Bytes.Num());
			Snapshot->Freeze();
		}
	};

	FString CaptureTokenProjection(
		const FLexerSource& Source,
		const AS_NAMESPACE_QUALIFIER asSLexOptions& Options,
		AS_NAMESPACE_QUALIFIER asCIdentifierTable& IdentifierTable,
		TArray<AS_NAMESPACE_QUALIFIER asCToken>* OutTokens = nullptr)
	{
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			Options,
			IdentifierTable,
			Diagnostics);
		FString Projection;
		for (;;)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!Tokenizer.Lex(Token))
			{
				return TEXT("<lex-failed>");
			}
			Projection += FString::Printf(
				TEXT("%s:%u-%u:%u\n"),
				AS_NAMESPACE_QUALIFIER asGetTokenKindName(Token.Kind),
				Token.Range.Begin.ByteOffset,
				Token.Range.End.ByteOffset,
				static_cast<asUINT>(Token.Flags));
			if (OutTokens != nullptr)
			{
				OutTokens->Add(Token);
			}
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				break;
			}
		}
		return Projection;
	}

	FString MakeRepresentativeLexerCorpus()
	{
		FString Corpus;
		constexpr int32 RepeatCount = 256;
		constexpr FStringView Sample =
			TEXTVIEW("namespace Bench { class Sample { UPROPERTY int Count = 42; "
				"void Tick(float Delta) { Count += 1; if (Count > 100) { Count = 0; } } } }\n");
		Corpus.Reserve(Sample.Len() * RepeatCount);
		for (int32 Index = 0; Index < RepeatCount; ++Index)
		{
			Corpus.Append(Sample);
		}
		return Corpus;
	}

	struct FLexerMeasurement
	{
		bool bSucceeded = false;
		asUINT TokenCount = 0;
		double ElapsedMicroseconds = 0.0;
		FPlatformMemoryStats MemoryBefore;
		FPlatformMemoryStats MemoryAfter;
	};

	FLexerMeasurement MeasureLexer(
		const FLexerSource& Source,
		AS_NAMESPACE_QUALIFIER asCIdentifierTable& IdentifierTable)
	{
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		FLexerMeasurement Result;
		Result.MemoryBefore = FPlatformMemory::GetStats();
		const double StartSeconds = FPlatformTime::Seconds();
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			IdentifierTable,
			Diagnostics);
		for (asUINT Guard = 0; Guard <= Source.ByteCount; ++Guard)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!Tokenizer.Lex(Token)) { break; }
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				Result.bSucceeded = true;
				break;
			}
			if (Token.Range.Length() == 0) { break; }
			++Result.TokenCount;
		}
		Result.ElapsedMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Result.MemoryAfter = FPlatformMemory::GetStats();
		return Result;
	}
}

static_assert(std::is_copy_constructible_v<AS_NAMESPACE_QUALIFIER asSLexOptions>);
static_assert(!std::is_copy_assignable_v<AS_NAMESPACE_QUALIFIER asSLexOptions>);
static_assert(!std::is_constructible_v<AS_NAMESPACE_QUALIFIER asSLexOptions, const asCScriptEngine*>);
static_assert(std::is_trivially_copyable_v<AS_NAMESPACE_QUALIFIER asCToken>);
static_assert(sizeof(AS_NAMESPACE_QUALIFIER asCToken) <= 64);
static_assert(!std::is_constructible_v<AS_NAMESPACE_QUALIFIER asCTokenizer, const asCScriptEngine*>);

TEST_CLASS_WITH_FLAGS(
	Lexer,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FrozenOptionsAreValueOwned)
	{
		const AS_NAMESPACE_QUALIFIER asSLexOptions Options(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia);
		ASSERT_THAT(IsTrue(Options.AllowUnicodeIdentifiers()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia, Options.GetTriviaMode()));
	}

	TEST_METHOD(CharacterStreamUsesValidatedSnapshotRange)
	{
		const FLexerSource Source(TEXT("prefix body suffix"));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCCharacterStream Stream;
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCCharacterStream::TryCreate(
			Manager,
			Manager.MakeRange(Source.FileID, 7, 11),
			Stream)));

		ASSERT_THAT(AreEqual(static_cast<uint8>('b'), Stream.Peek()));
		ASSERT_THAT(AreEqual(7u, Stream.GetByteOffset()));
		ASSERT_THAT(IsTrue(Stream.Advance(4)));
		ASSERT_THAT(IsTrue(Stream.IsAtEnd()));
		ASSERT_THAT(AreEqual(11u, Stream.GetByteOffset()));
		ASSERT_THAT(IsFalse(Stream.Advance(1)));

		AS_NAMESPACE_QUALIFIER asCCharacterStream Invalid;
		ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asCCharacterStream::TryCreate(
			Manager,
			Manager.MakeRange(Source.FileID, 0, 32),
			Invalid)));
	}

	TEST_METHOD(TokenContractIsSourceReferentialAndDeclarative)
	{
		const FLexerSource Source(TEXT("class"));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCToken Token;
		Token.Kind = AS_NAMESPACE_QUALIFIER asETokenKind::KwClass;
		Token.Range = Manager.MakeRange(Source.FileID, 0, 5);
		Token.Flags = AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine;

		ASSERT_THAT(AreEqual(FString(TEXT("class")), FString(AS_NAMESPACE_QUALIFIER asGetTokenSpelling(Token.Kind))));
		ASSERT_THAT(AreEqual(FString(TEXT("KwClass")), FString(AS_NAMESPACE_QUALIFIER asGetTokenKindName(Token.Kind))));
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(Token.Kind)));
		ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asIsTrivia(Token.Kind)));
		ASSERT_THAT(AreEqual(5u, Token.Range.Length()));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Token.Flags, AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine)));
	}

	TEST_METHOD(PullLexerClassifiesKeywordsReflectionSpellingsLiteralsAndPunctuation)
	{
		const FLexerSource Source(TEXT("class Thing {\n  UPROPERTY int value = 42;\n  string name = \"AS\"; Thing@ other;\n}"));
		const AS_NAMESPACE_QUALIFIER asSLexOptions Options;
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		const FString Projection = CaptureTokenProjection(Source, Options, IdentifierTable, &Tokens);
		ASSERT_THAT(IsFalse(Projection.Contains(TEXT("<lex-failed>"))));

		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::KwClass, Tokens[0].Kind));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Tokens[0].Flags, AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[1].Kind));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Tokens[1].Flags, AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::LeftBrace, Tokens[2].Kind));

		const AS_NAMESPACE_QUALIFIER asCToken* UProperty = Tokens.FindByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::KwUProperty;
		});
		ASSERT_THAT(IsNotNull(UProperty));
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(UProperty->Kind)));

		ASSERT_THAT(IsTrue(Tokens.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral && Token.Range.Length() == 2;
		})));
		ASSERT_THAT(IsTrue(Tokens.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral && Token.Range.Length() == 4;
		})));
		ASSERT_THAT(IsTrue(Tokens.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::At;
		})));
	}

	TEST_METHOD(OnlySixOuterAnnotationsAndCallableIntroducersAreKeywords)
	{
		const FLexerSource Source(TEXT("UCLASS USTRUCT UENUM UFUNCTION UPROPERTY UMETA delegate event Meta Category DisplayName UDELEGATE asset of import from"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), Table, &Tokens);
		const AS_NAMESPACE_QUALIFIER asETokenKind Expected[] = {
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUClass, AS_NAMESPACE_QUALIFIER asETokenKind::KwUStruct, AS_NAMESPACE_QUALIFIER asETokenKind::KwUEnum,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUFunction, AS_NAMESPACE_QUALIFIER asETokenKind::KwUProperty, AS_NAMESPACE_QUALIFIER asETokenKind::KwUMeta,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwDelegate, AS_NAMESPACE_QUALIFIER asETokenKind::KwEvent };
		ASSERT_THAT(AreEqual(17, Tokens.Num()));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index], Tokens[Index].Kind));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(Tokens[Index].Kind)));
		}
		for (int32 Index = 8; Index < Tokens.Num() - 1; ++Index)
		{
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[Index].Kind));
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEReflectionSpelling::None, Tokens[Index].Identifier->GetReflectionKind()));
		}
	}


	TEST_METHOD(LeadingDotExponentAndRadixNumbersRetainWholeTokens)
	{
		const FLexerSource Source(TEXT(".5 1. 1e+2f 0x2A 0b101 0o52 0d42 123abc 0b102 1e+"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), Table, &Tokens);
		const asUINT Lengths[] = {2, 2, 5, 4, 5, 4, 4, 6, 5, 3};
		ASSERT_THAT(AreEqual(11, Tokens.Num()));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lengths); ++Index)
		{
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[Index].Kind));
			ASSERT_THAT(AreEqual(Lengths[Index], Tokens[Index].Range.Length()));
		}
	}

	TEST_METHOD(HexadecimalEDigitDoesNotAbsorbAdjacentAddition)
	{
		const FLexerSource Source(TEXT("0x1e+2 . Member"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), Table, &Tokens);
		ASSERT_THAT(AreEqual(6, Tokens.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[0].Kind));
		ASSERT_THAT(AreEqual(asUINT(4), Tokens[0].Range.Length()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Plus, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[2].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Dot, Tokens[3].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[4].Kind));
	}


	TEST_METHOD(RepeatedIdentifiersReuseOneSessionEntry)
	{
		const FLexerSource Source(TEXT("Thing Thing Thing"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), IdentifierTable, &Tokens);

		ASSERT_THAT(AreEqual(4, Tokens.Num()));
		ASSERT_THAT(IsNotNull(Tokens[0].Identifier));
		ASSERT_THAT(IsTrue(Tokens[0].Identifier == Tokens[1].Identifier));
		ASSERT_THAT(IsTrue(Tokens[1].Identifier == Tokens[2].Identifier));
		ASSERT_THAT(AreEqual(1u, IdentifierTable.GetEntryCount()));
		ASSERT_THAT(AreEqual(1u, IdentifierTable.GetUniqueEntryAllocationCount()));
	}

	TEST_METHOD(HeredocIsOneLiteralAcrossNewlinesAndEmbeddedQuotes)
	{
		const FString Literal = TEXT("\"\"\"line\n\"quoted\" \\n\"\"\"");
		const FLexerSource Source(Literal + TEXT("; After"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), Table, &Tokens);
		ASSERT_THAT(AreEqual(4, Tokens.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral, Tokens[0].Kind));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Literal.Len()), Tokens[0].Range.Length()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Semicolon, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[2].Kind));
	}

	TEST_METHOD(PostfixAndNestedGenericPunctuationPreserveMaximalMunch)
	{
		const FLexerSource Source(TEXT("Value.Member(Arg: 2)[0]++ array<array<int>>"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), Table, &Tokens);
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Dot, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Colon, Tokens[5].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::PlusPlus, Tokens[11].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRight, Tokens[17].Kind));
		ASSERT_THAT(AreEqual(asUINT(2), Tokens[17].Range.Length()));
	}

	TEST_METHOD(IndependentSessionsProduceIdenticalTokenProjection)
	{
		const FLexerSource Source(TEXT("namespace Demo { int Count = 0x2A; Count += 1; }"));
		AS_NAMESPACE_QUALIFIER asCIdentifierTable FirstTable;
		AS_NAMESPACE_QUALIFIER asCIdentifierTable SecondTable;
		const FString First = CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), FirstTable);
		const FString Second = CaptureTokenProjection(Source, AS_NAMESPACE_QUALIFIER asSLexOptions(), SecondTable);
		ASSERT_THAT(AreEqual(First, Second));
		ASSERT_THAT(IsFalse(First.Contains(TEXT("Whitespace"))));
	}

	TEST_METHOD(UnicodePolicyIsFrozenPerTokenizer)
	{
		const FLexerSource Source(TEXT("\u03c0Value"));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);

		AS_NAMESPACE_QUALIFIER asCIdentifierTable UnicodeTable;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine UnicodeDiagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCTokenizer UnicodeTokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			AS_NAMESPACE_QUALIFIER asSLexOptions(AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode),
			UnicodeTable,
			UnicodeDiagnostics);
		AS_NAMESPACE_QUALIFIER asCToken UnicodeToken;
		ASSERT_THAT(IsTrue(UnicodeTokenizer.Lex(UnicodeToken)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, UnicodeToken.Kind));
		ASSERT_THAT(AreEqual(Source.ByteCount, UnicodeToken.Range.Length()));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(UnicodeToken.Flags, AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode)));

		AS_NAMESPACE_QUALIFIER asCIdentifierTable AsciiTable;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine AsciiDiagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCTokenizer AsciiTokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			AS_NAMESPACE_QUALIFIER asSLexOptions(AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly),
			AsciiTable,
			AsciiDiagnostics);
		AS_NAMESPACE_QUALIFIER asCToken AsciiToken;
		ASSERT_THAT(IsTrue(AsciiTokenizer.Lex(AsciiToken)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, AsciiToken.Kind));
		ASSERT_THAT(AreEqual(2u, AsciiToken.Range.Length()));
		ASSERT_THAT(IsTrue(AsciiTokenizer.FlushDiagnostics()));
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		AsciiDiagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords().Num()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnicodeIdentifierDisallowed),
			Consumer.GetRecords()[0].DiagnosticID));
	}

	TEST_METHOD(MalformedBytesAlwaysAdvanceAndDiagnoseOnce)
	{
		const uint8 Bytes[] = {0xFF, 0x00, '$', '"', 'x'};
		const FLexerSource Source(MakeArrayView(Bytes));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			IdentifierTable,
			Diagnostics);

		TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		for (int32 Guard = 0; Guard < 16; ++Guard)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			ASSERT_THAT(IsTrue(Tokenizer.Lex(Token)));
			Tokens.Add(Token);
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile) { break; }
			ASSERT_THAT(IsTrue(Token.Range.Length() > 0, TEXT("Every non-EOF pull must consume bytes.")));
		}
		ASSERT_THAT(AreEqual(5, Tokens.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, Tokens.Last().Kind));
		ASSERT_THAT(AreEqual(Source.ByteCount, Tokens.Last().Range.Begin.ByteOffset));

		AS_NAMESPACE_QUALIFIER asCToken RepeatedEOF;
		ASSERT_THAT(IsTrue(Tokenizer.Lex(RepeatedEOF)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, RepeatedEOF.Kind));
		ASSERT_THAT(AreEqual(0u, RepeatedEOF.Range.Length()));

		ASSERT_THAT(IsTrue(Tokenizer.FlushDiagnostics()));
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(4, Consumer.GetRecords().Num()));
	}

	TEST_METHOD(UnterminatedBlockCommentConsumesToEnd)
	{
		const FLexerSource Source(TEXT("/* unfinished"));
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Snapshot);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, Source.ByteCount),
			AS_NAMESPACE_QUALIFIER asSLexOptions(
				AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
				AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
			IdentifierTable,
			Diagnostics);
		AS_NAMESPACE_QUALIFIER asCToken Comment;
		ASSERT_THAT(IsTrue(Tokenizer.Lex(Comment)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::BlockComment, Comment.Kind));
		ASSERT_THAT(AreEqual(Source.ByteCount, Comment.Range.Length()));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(Comment.Flags, AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated)));
		ASSERT_THAT(IsTrue(Tokenizer.FlushDiagnostics()));
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
		Diagnostics.EmitTo(Consumer);
		ASSERT_THAT(AreEqual(1, Consumer.GetRecords().Num()));
	}

	TEST_METHOD(TriviaModesPreserveNonTriviaRanges)
	{
		const FLexerSource Source(TEXT(" \n// note\n# value"));
		auto Capture = [&Source](AS_NAMESPACE_QUALIFIER asETriviaMode Mode, TArray<AS_NAMESPACE_QUALIFIER asCToken>& Tokens)
		{
			AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
			CaptureTokenProjection(
				Source,
				AS_NAMESPACE_QUALIFIER asSLexOptions(AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly, Mode),
				Table,
				&Tokens);
		};

		TArray<AS_NAMESPACE_QUALIFIER asCToken> Skipped;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Retained;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> Raw;
		Capture(AS_NAMESPACE_QUALIFIER asETriviaMode::SkipTrivia, Skipped);
		Capture(AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia, Retained);
		Capture(AS_NAMESPACE_QUALIFIER asETriviaMode::RawDirective, Raw);
		ASSERT_THAT(IsFalse(Skipped.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return AS_NAMESPACE_QUALIFIER asIsTrivia(Token.Kind);
		})));
		ASSERT_THAT(IsTrue(Retained.ContainsByPredicate([](const AS_NAMESPACE_QUALIFIER asCToken& Token)
		{
			return Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::LineComment;
		})));
		ASSERT_THAT(AreEqual(Retained.Num(), Raw.Num()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Hash, Skipped[0].Kind));
		ASSERT_THAT(AreEqual(Skipped[0].Range.Begin.FileID.Value, Retained[3].Range.Begin.FileID.Value));
		ASSERT_THAT(AreEqual(Skipped[0].Range.Begin.ByteOffset, Retained[3].Range.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(Skipped[0].Range.End.ByteOffset, Retained[3].Range.End.ByteOffset));
	}

	TEST_METHOD(IndependentParallelSessionsRemainDeterministic)
	{
		const FLexerSource Source(TEXT("class Parallel { UFUNCTION void Run(int Value); }"));
		TArray<FString> Projections;
		Projections.SetNum(16);
		ParallelFor(Projections.Num(), [&Source, &Projections](int32 Index)
		{
			AS_NAMESPACE_QUALIFIER asCIdentifierTable Table;
			Projections[Index] = CaptureTokenProjection(
				Source,
				AS_NAMESPACE_QUALIFIER asSLexOptions(AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode),
				Table);
		});
		for (int32 Index = 1; Index < Projections.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(Projections[0], Projections[Index]));
		}
	}

	TEST_METHOD(RepresentativeCorpusRecordsColdAndWarmEvidence)
	{
		const FString Corpus = MakeRepresentativeLexerCorpus();
		const FLexerSource Source(Corpus);
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;

		const asUINT AllocationsBeforeCold = IdentifierTable.GetUniqueEntryAllocationCount();
		const FLexerMeasurement Cold = MeasureLexer(Source, IdentifierTable);
		const asUINT AllocationsAfterCold = IdentifierTable.GetUniqueEntryAllocationCount();
		const asUINT AllocationsBeforeWarm = AllocationsAfterCold;
		const FLexerMeasurement Warm = MeasureLexer(Source, IdentifierTable);
		const asUINT AllocationsAfterWarm = IdentifierTable.GetUniqueEntryAllocationCount();

		ASSERT_THAT(IsTrue(Cold.bSucceeded));
		ASSERT_THAT(IsTrue(Warm.bSucceeded));
		ASSERT_THAT(IsTrue(Cold.TokenCount > 0));
		ASSERT_THAT(AreEqual(Cold.TokenCount, Warm.TokenCount));
		ASSERT_THAT(IsTrue(AllocationsAfterCold > AllocationsBeforeCold));
		ASSERT_THAT(IsTrue(AllocationsAfterCold < Cold.TokenCount));
		ASSERT_THAT(AreEqual(AllocationsBeforeWarm, AllocationsAfterWarm));

		UE_LOG(
			LogTemp,
			Display,
			TEXT("AS_FRONTEND_LEXER_BASELINE corpus=v1 bytes=%u tokens=%u cold_new_identifier_allocations=%u warm_new_identifier_allocations=%u unique_identifiers=%u owned_spelling_bytes=%u cold_used_physical_before=%llu cold_used_physical_after=%llu cold_peak_used_physical=%llu warm_used_physical_before=%llu warm_used_physical_after=%llu warm_peak_used_physical=%llu cold_elapsed_us=%.3f warm_elapsed_us=%.3f"),
			Source.ByteCount,
			Cold.TokenCount,
			AllocationsAfterCold - AllocationsBeforeCold,
			AllocationsAfterWarm - AllocationsBeforeWarm,
			IdentifierTable.GetEntryCount(),
			IdentifierTable.GetOwnedSpellingByteCount(),
			static_cast<unsigned long long>(Cold.MemoryBefore.UsedPhysical),
			static_cast<unsigned long long>(Cold.MemoryAfter.UsedPhysical),
			static_cast<unsigned long long>(Cold.MemoryAfter.PeakUsedPhysical),
			static_cast<unsigned long long>(Warm.MemoryBefore.UsedPhysical),
			static_cast<unsigned long long>(Warm.MemoryAfter.UsedPhysical),
			static_cast<unsigned long long>(Warm.MemoryAfter.PeakUsedPhysical),
			Cold.ElapsedMicroseconds,
			Warm.ElapsedMicroseconds);
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
