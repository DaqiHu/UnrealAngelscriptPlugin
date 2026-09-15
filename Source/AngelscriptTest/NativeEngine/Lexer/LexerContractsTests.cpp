#include "CQTest.h"
#include "TestFramework/NativeEngine/NativeEngineTokenizerTest.h"

#include "frontend/Basic/as_character_stream.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/Lexer/as_token.h"

#include "Async/ParallelFor.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Templates/SharedPointer.h"

#include <string>
#include <type_traits>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

static_assert(std::is_copy_constructible_v<AS_NAMESPACE_QUALIFIER asSLexOptions>);
static_assert(!std::is_copy_assignable_v<AS_NAMESPACE_QUALIFIER asSLexOptions>);
static_assert(!std::is_constructible_v<AS_NAMESPACE_QUALIFIER asSLexOptions, const asCScriptEngine*>);
static_assert(std::is_trivially_copyable_v<AS_NAMESPACE_QUALIFIER asCToken>);
static_assert(sizeof(AS_NAMESPACE_QUALIFIER asCToken) <= 64);
static_assert(!std::is_constructible_v<AS_NAMESPACE_QUALIFIER asCTokenizer, const asCScriptEngine*>);

TEST_CLASS_WITH_FLAGS(
	Contracts,
	"Angelscript.UnitTest.NativeEngine.Lexer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeEngineTokenizerTest = LexerTest::FNativeEngineTokenizerTest;
	using FExpectedToken = LexerTest::FExpectedToken;
	using FLexExpectation = LexerTest::FLexExpectation;

	struct FMeasurementSource
	{
		std::string Bytes;
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		AS_NAMESPACE_QUALIFIER asCSourceManager Manager;

		explicit FMeasurementSource(std::string InBytes)
			: Bytes(std::move(InBytes))
			, Manager(LexerTest::MakeOwnedSourceManager(
				TEXTVIEW("/Angelscript/Test/LexerMeasurement.as"),
				Bytes,
				&FileID))
		{
			check(Bytes.size() <= static_cast<size_t>(MAX_int32));
		}
	};

	struct FLexerMeasurement
	{
		bool bSucceeded = false;
		asUINT TokenCount = 0;
		double ElapsedMicroseconds = 0.0;
		FPlatformMemoryStats MemoryBefore;
		FPlatformMemoryStats MemoryAfter;
	};

	static FString MakeRepresentativeLexerCorpus()
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

	static FLexerMeasurement MeasureLexer(
		const FMeasurementSource& Source,
		AS_NAMESPACE_QUALIFIER asCIdentifierTable& IdentifierTable)
	{
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Source.Manager);
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics(Manager);
		FLexerMeasurement Result;
		Result.MemoryBefore = FPlatformMemory::GetStats();
		const double StartSeconds = FPlatformTime::Seconds();
		AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
			Manager,
			Manager.MakeRange(Source.FileID, 0, static_cast<asUINT>(Source.Bytes.size())),
			AS_NAMESPACE_QUALIFIER asSLexOptions(),
			IdentifierTable,
			Diagnostics);
		for (size_t Guard = 0; Guard <= Source.Bytes.size(); ++Guard)
		{
			AS_NAMESPACE_QUALIFIER asCToken Token;
			if (!Tokenizer.Lex(Token))
			{
				break;
			}
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				Result.bSucceeded = true;
				break;
			}
			if (Token.Range.Length() == 0)
			{
				break;
			}
			++Result.TokenCount;
		}
		Result.ElapsedMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Result.MemoryAfter = FPlatformMemory::GetStats();
		return Result;
	}

public:
	TEST_METHOD(FrozenOptionsAreValueOwned)
	{
		const AS_NAMESPACE_QUALIFIER asSLexOptions Options(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia);
		ASSERT_THAT(IsTrue(Options.AllowUnicodeIdentifiers()));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia,
			Options.GetTriviaMode()));

		FNativeEngineTokenizerTest Text = FNativeEngineTokenizerTest::FromText(TEXTVIEW("class X"));
		ASSERT_THAT(IsTrue(Text.GetSourceBytes().size() == 7));
		if (!Text.CheckKinds(
			Assert,
			{
				AS_NAMESPACE_QUALIFIER asETokenKind::KwClass,
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
			},
			"text input"))
		{
			return;
		}

		FNativeEngineTokenizerTest Bytes = FNativeEngineTokenizerTest::FromUtf8Bytes("++");
		ASSERT_THAT(IsTrue(Bytes.GetSourceBytes().size() == 2));
		if (!Bytes.CheckKinds(
			Assert,
			{
				AS_NAMESPACE_QUALIFIER asETokenKind::PlusPlus,
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
			},
			"byte input"))
		{
			return;
		}

		FNativeEngineTokenizerTest Unicode = FNativeEngineTokenizerTest::FromText(TEXTVIEW("\u03c0"));
		{
			const AS_NAMESPACE_QUALIFIER asSLexOptions LocalOptions(
				AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode);
			Unicode.WithOptions(LocalOptions);
		}
		ASSERT_THAT(IsTrue(Unicode.GetSourceBytes().size() == 2));
		if (!Unicode.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
						0,
						2,
						AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 2, 2},
				},
				{},
			},
			"block-local options"))
		{
			return;
		}
	}

	TEST_METHOD(CharacterStreamUsesValidatedSnapshotRange)
	{
		FNativeEngineTokenizerTest Owned =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("prefix body suffix"));
		const std::string_view Bytes = Owned.GetSourceBytes();
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		const AS_NAMESPACE_QUALIFIER asCSourceManager Manager = LexerTest::MakeOwnedSourceManager(
			TEXTVIEW("/Angelscript/Test/CharacterStream.as"),
			Bytes,
			&FileID);
		AS_NAMESPACE_QUALIFIER asCCharacterStream Stream;
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asCCharacterStream::TryCreate(
			Manager,
			Manager.MakeRange(FileID, 7, 11),
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
			Manager.MakeRange(FileID, 0, 32),
			Invalid)));
	}

	TEST_METHOD(TokenContractIsSourceReferentialAndDeclarative)
	{
		FNativeEngineTokenizerTest Lexer =
			FNativeEngineTokenizerTest::FromUtf8Bytes("class Name 42 >>");
		const FLexExpectation Expected{
			{
				FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::KwClass, 0, 5},
				FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, 6, 10},
				FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, 11, 13},
				FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRight, 14, 16},
				FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 16, 16},
			},
			{},
		};
		if (!Lexer.Check(Assert, Expected, "authored source ranges"))
		{
			return;
		}

		constexpr std::string_view Spellings[] = {"class", "Name", "42", ">>"};
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		for (size_t Index = 0; Index < std::size(Spellings); ++Index)
		{
			const AS_NAMESPACE_QUALIFIER asCToken& Token = Tokens[Index];
			const std::string_view Actual = Lexer.GetSourceBytes().substr(
				Token.Range.Begin.ByteOffset,
				Token.Range.Length());
			ASSERT_THAT(IsTrue(Actual == Spellings[Index], TEXT("Token range must retain authored bytes.")));
		}
		ASSERT_THAT(AreEqual(
			FString(TEXT("class")),
			FString(AS_NAMESPACE_QUALIFIER asGetTokenSpelling(Tokens[0].Kind))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("KwClass")),
			FString(AS_NAMESPACE_QUALIFIER asGetTokenKindName(Tokens[0].Kind))));
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(Tokens[0].Kind)));
		ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asIsTrivia(Tokens[0].Kind)));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(
			Tokens[0].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine)));
		ASSERT_THAT(IsTrue(Lexer.Describe().find("KwClass") != std::string::npos));
	}

	TEST_METHOD(PullLexerClassifiesKeywordsReflectionSpellingsLiteralsAndPunctuation)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(
			TEXTVIEW("class Thing {\n  UPROPERTY int value = 42;\n  string name = \"AS\"; Thing@ other;\n}"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		ASSERT_THAT(IsTrue(Tokens.size() > 3));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::KwClass, Tokens[0].Kind));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(
			Tokens[0].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[1].Kind));
		ASSERT_THAT(IsTrue(EnumHasAnyFlags(
			Tokens[1].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::LeftBrace, Tokens[2].Kind));

		const AS_NAMESPACE_QUALIFIER asCToken* UProperty = nullptr;
		bool bHasNumericLiteral = false;
		bool bHasStringLiteral = false;
		bool bHasAt = false;
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Tokens)
		{
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::KwUProperty)
			{
				UProperty = &Token;
			}
			bHasNumericLiteral |= Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral
				&& Token.Range.Length() == 2;
			bHasStringLiteral |= Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral
				&& Token.Range.Length() == 4;
			bHasAt |= Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::At;
		}
		ASSERT_THAT(IsNotNull(UProperty));
		if (UProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(UProperty->Kind)));
		ASSERT_THAT(IsTrue(bHasNumericLiteral));
		ASSERT_THAT(IsTrue(bHasStringLiteral));
		ASSERT_THAT(IsTrue(bHasAt));
	}

	TEST_METHOD(OnlySixOuterAnnotationsAndCallableIntroducersAreKeywords)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(TEXTVIEW(
			"UCLASS USTRUCT UENUM UFUNCTION UPROPERTY UMETA delegate event "
			"Meta Category DisplayName UDELEGATE asset of import from"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		const AS_NAMESPACE_QUALIFIER asETokenKind Expected[] = {
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUClass,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUStruct,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUEnum,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUFunction,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUProperty,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUMeta,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwDelegate,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwEvent,
		};
		ASSERT_THAT(IsTrue(Tokens.size() == 17));
		for (size_t Index = 0; Index < std::size(Expected); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index], Tokens[Index].Kind));
			ASSERT_THAT(IsTrue(AS_NAMESPACE_QUALIFIER asIsKeyword(Tokens[Index].Kind)));
		}
		for (size_t Index = 8; Index + 1 < Tokens.size(); ++Index)
		{
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[Index].Kind));
			ASSERT_THAT(IsNotNull(Tokens[Index].Identifier));
			if (Tokens[Index].Identifier != nullptr)
			{
				ASSERT_THAT(AreEqual(
					AS_NAMESPACE_QUALIFIER asEReflectionSpelling::None,
					Tokens[Index].Identifier->GetReflectionKind()));
			}
		}
	}

	TEST_METHOD(LeadingDotExponentAndRadixNumbersRetainWholeTokens)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(
			TEXTVIEW(".5 1. 1e+2f 0x2A 0b101 0o52 0d42 123abc 0b102 1e+"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		const asUINT Lengths[] = {2, 2, 5, 4, 5, 4, 4, 6, 5, 3};
		ASSERT_THAT(IsTrue(Tokens.size() == 11));
		for (size_t Index = 0; Index < std::size(Lengths); ++Index)
		{
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[Index].Kind));
			ASSERT_THAT(AreEqual(Lengths[Index], Tokens[Index].Range.Length()));
		}
	}

	TEST_METHOD(HexadecimalEDigitDoesNotAbsorbAdjacentAddition)
	{
		FNativeEngineTokenizerTest Lexer =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("0x1e+2 . Member"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		ASSERT_THAT(IsTrue(Tokens.size() == 6));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[0].Kind));
		ASSERT_THAT(AreEqual(asUINT(4), Tokens[0].Range.Length()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Plus, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::NumericLiteral, Tokens[2].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Dot, Tokens[3].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[4].Kind));
	}

	TEST_METHOD(RepeatedIdentifiersReuseOneSessionEntry)
	{
		FNativeEngineTokenizerTest Lexer =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("Thing Thing Thing"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		ASSERT_THAT(IsTrue(Tokens.size() == 4));
		ASSERT_THAT(IsNotNull(Tokens[0].Identifier));
		ASSERT_THAT(IsTrue(Tokens[0].Identifier == Tokens[1].Identifier));
		ASSERT_THAT(IsTrue(Tokens[1].Identifier == Tokens[2].Identifier));
	}

	TEST_METHOD(HeredocIsOneLiteralAcrossNewlinesAndEmbeddedQuotes)
	{
		constexpr FStringView Literal = TEXTVIEW("\"\"\"line\n\"quoted\" \\n\"\"\"");
		FString Source(Literal);
		Source += TEXT("; After");
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(Source);
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		ASSERT_THAT(IsTrue(Tokens.size() == 4));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral, Tokens[0].Kind));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Literal.Len()), Tokens[0].Range.Length()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Semicolon, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, Tokens[2].Kind));
	}

	TEST_METHOD(PostfixAndNestedGenericPunctuationPreserveMaximalMunch)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(
			TEXTVIEW("Value.Member(Arg: 2)[0]++ array<array<int>>"));
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		ASSERT_THAT(IsTrue(Tokens.size() > 18));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Dot, Tokens[1].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Colon, Tokens[5].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::PlusPlus, Tokens[11].Kind));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRight, Tokens[17].Kind));
		ASSERT_THAT(AreEqual(asUINT(2), Tokens[17].Range.Length()));
	}

	TEST_METHOD(IndependentSessionsProduceIdenticalTokenProjection)
	{
		constexpr FStringView Source = TEXTVIEW("namespace Demo { int Count = 0x2A; Count += 1; }");
		FNativeEngineTokenizerTest First = FNativeEngineTokenizerTest::FromText(Source);
		FNativeEngineTokenizerTest Second = FNativeEngineTokenizerTest::FromText(Source);
		const std::string FirstProjection = First.Describe();
		const std::string SecondProjection = Second.Describe();
		ASSERT_THAT(IsTrue(FirstProjection == SecondProjection));
		ASSERT_THAT(IsTrue(FirstProjection.find("Whitespace") == std::string::npos));
	}

	TEST_METHOD(UnicodePolicyIsFrozenPerTokenizer)
	{
		FNativeEngineTokenizerTest Unicode =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("\u03c0Value"));
		Unicode.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode));
		if (!Unicode.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
						0,
						7,
						AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 7, 7},
				},
				{},
			},
			"allow Unicode"))
		{
			return;
		}

		FNativeEngineTokenizerTest Ascii =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("\u03c0Value"));
		if (!Ascii.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 0, 2},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, 2, 7},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 7, 7},
				},
				{
					LexerTest::FExpectedLexDiagnostic{
						AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnicodeIdentifierDisallowed,
						0,
						2},
				},
			},
			"ASCII-only Unicode"))
		{
			return;
		}
	}

	TEST_METHOD(MalformedBytesAlwaysAdvanceAndDiagnoseOnce)
	{
		const std::string Bytes("\xFF\x00$\"x", 5);
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes(Bytes);
		if (!Lexer.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 0, 1},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 1, 2},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 2, 3},
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral,
						3,
						5,
						AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated},
					FExpectedToken{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 5, 5},
				},
				{
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::InvalidUtf8, 0, 1},
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::EmbeddedNul, 1, 2},
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnexpectedCharacter, 2, 3},
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnterminatedString, 3, 5},
				},
			},
			"mixed malformed bytes"))
		{
			return;
		}
		const AS_NAMESPACE_QUALIFIER asCToken& RepeatedEOF = Lexer.GetRepeatedEndOfFile();
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, RepeatedEOF.Kind));
		ASSERT_THAT(AreEqual(5u, RepeatedEOF.Range.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(0u, RepeatedEOF.Range.Length()));
	}

	TEST_METHOD(UnterminatedBlockCommentConsumesToEnd)
	{
		constexpr std::string_view Source = "/* unfinished";
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes(Source);
		Lexer.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		if (!Lexer.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::BlockComment,
						0,
						static_cast<asUINT>(Source.size()),
						AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated},
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
						static_cast<asUINT>(Source.size()),
						static_cast<asUINT>(Source.size())},
				},
				{
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnterminatedBlockComment,
						0,
						static_cast<asUINT>(Source.size())},
				},
			},
			"unterminated block comment"))
		{
			return;
		}
	}

	TEST_METHOD(TriviaModesPreserveNonTriviaRanges)
	{
		constexpr FStringView Source = TEXTVIEW(" \n// note\n# value");
		FNativeEngineTokenizerTest Skipped = FNativeEngineTokenizerTest::FromText(Source);
		FNativeEngineTokenizerTest Retained = FNativeEngineTokenizerTest::FromText(Source);
		Retained.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		FNativeEngineTokenizerTest Raw = FNativeEngineTokenizerTest::FromText(Source);
		Raw.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RawDirective));

		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> SkippedTokens = Skipped.GetTokens();
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> RetainedTokens = Retained.GetTokens();
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> RawTokens = Raw.GetTokens();
		bool bSkippedHasTrivia = false;
		bool bRetainedHasLineComment = false;
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : SkippedTokens)
		{
			bSkippedHasTrivia |= AS_NAMESPACE_QUALIFIER asIsTrivia(Token.Kind);
		}
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : RetainedTokens)
		{
			bRetainedHasLineComment |=
				Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::LineComment;
		}
		ASSERT_THAT(IsFalse(bSkippedHasTrivia));
		ASSERT_THAT(IsTrue(bRetainedHasLineComment));
		ASSERT_THAT(IsTrue(RetainedTokens.size() == RawTokens.size()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Hash, SkippedTokens[0].Kind));
		ASSERT_THAT(AreEqual(
			SkippedTokens[0].Range.Begin.FileID.Value,
			RetainedTokens[3].Range.Begin.FileID.Value));
		ASSERT_THAT(AreEqual(
			SkippedTokens[0].Range.Begin.ByteOffset,
			RetainedTokens[3].Range.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(
			SkippedTokens[0].Range.End.ByteOffset,
			RetainedTokens[3].Range.End.ByteOffset));
	}

	TEST_METHOD(IndependentParallelSessionsRemainDeterministic)
	{
		constexpr FStringView Source = TEXTVIEW("class Parallel { UFUNCTION void Run(int Value); }");
		std::vector<std::string> Projections(16);
		ParallelFor(static_cast<int32>(Projections.size()), [&Projections, Source](int32 Index)
		{
			FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromText(Source);
			Lexer.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
				AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode));
			Projections[Index] = Lexer.Describe();
		});
		for (size_t Index = 1; Index < Projections.size(); ++Index)
		{
			ASSERT_THAT(IsTrue(Projections[0] == Projections[Index]));
		}
	}

	TEST_METHOD(RepresentativeCorpusRecordsColdAndWarmEvidence)
	{
		const FString Corpus = MakeRepresentativeLexerCorpus();
		FNativeEngineTokenizerTest Encoded = FNativeEngineTokenizerTest::FromText(Corpus);
		const FMeasurementSource Source(std::string(Encoded.GetSourceBytes()));
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
			static_cast<asUINT>(Source.Bytes.size()),
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

	TEST_METHOD(EmptyAndTriviaOnlySourcesReachStableEOF)
	{
		FNativeEngineTokenizerTest Empty = FNativeEngineTokenizerTest::FromUtf8Bytes({});
		if (!Empty.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
						0,
						0,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine},
				},
				{},
			},
			"empty source"))
		{
			return;
		}

		FNativeEngineTokenizerTest Trivia = FNativeEngineTokenizerTest::FromUtf8Bytes(" \t\r\n");
		if (!Trivia.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
						4,
						4,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
							| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace},
				},
				{},
			},
			"trivia-only source"))
		{
			return;
		}
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
