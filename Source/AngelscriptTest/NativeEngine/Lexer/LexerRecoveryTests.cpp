#include "CQTest.h"
#include "TestFramework/NativeEngine/NativeEngineTokenizerTest.h"

#include "frontend/Lexer/as_token.h"

#include <string>
#include <string_view>
#include <vector>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	Recovery,
	"Angelscript.UnitTest.NativeEngine.Lexer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeEngineTokenizerTest = LexerTest::FNativeEngineTokenizerTest;
	using FExpectedToken = LexerTest::FExpectedToken;
	using FExpectedLexDiagnostic = LexerTest::FExpectedLexDiagnostic;
	using FLexExpectation = LexerTest::FLexExpectation;
	using FMalformedUtf8Case = LexerTest::FMalformedUtf8Case;

	static bool SameProjection(
		std::span<const AS_NAMESPACE_QUALIFIER asCToken> Left,
		std::span<const AS_NAMESPACE_QUALIFIER asCToken> Right)
	{
		if (Left.size() != Right.size())
		{
			return false;
		}
		for (size_t Index = 0; Index < Left.size(); ++Index)
		{
			if (Left[Index].Kind != Right[Index].Kind
				|| Left[Index].Range.Begin.ByteOffset != Right[Index].Range.Begin.ByteOffset
				|| Left[Index].Range.End.ByteOffset != Right[Index].Range.End.ByteOffset
				|| Left[Index].Flags != Right[Index].Flags)
			{
				return false;
			}
		}
		return true;
	}

public:
	TEST_METHOD(MalformedGeneratorIsStableAndIndependent)
	{
		const std::vector<FMalformedUtf8Case> Expected = {
			{"truncated-2-1", std::string("\xC2", 1)},
			{"truncated-3-1", std::string("\xE2", 1)},
			{"truncated-3-2", std::string("\xE2\x82", 2)},
			{"truncated-4-1", std::string("\xF0", 1)},
			{"truncated-4-2", std::string("\xF0\x90", 2)},
			{"truncated-4-3", std::string("\xF0\x90\x80", 3)},
			{"bad-continuation-2", std::string("\xC2\x20", 2)},
			{"bad-continuation-3", std::string("\xE2\x20\xAC", 3)},
			{"bad-continuation-4", std::string("\xF0\x20\x80\x80", 4)},
			{"invalid-continuation-leader", std::string("\x80", 1)},
			{"invalid-leader", std::string("\xFF", 1)},
			{"overlong-2", std::string("\xC0\xAF", 2)},
			{"overlong-3", std::string("\xE0\x80\xAF", 3)},
			{"overlong-4", std::string("\xF0\x80\x80\xAF", 4)},
			{"surrogate", std::string("\xED\xA0\x80", 3)},
			{"above-unicode-max", std::string("\xF4\x90\x80\x80", 4)},
		};
		const std::vector<FMalformedUtf8Case> Actual =
			FNativeEngineTokenizerTest::GenerateMalformedUtf8Cases();
		ASSERT_THAT(IsTrue(Actual.size() == 16));
		ASSERT_THAT(IsTrue(Actual.size() == Expected.size()));
		if (Actual.size() != Expected.size())
		{
			return;
		}

		constexpr uint64 OffsetBasis = 14695981039346656037ull;
		constexpr uint64 Prime = 1099511628211ull;
		uint64 Fingerprint = OffsetBasis;
		auto Mix = [&Fingerprint](uint8 Byte)
		{
			Fingerprint ^= Byte;
			Fingerprint *= Prime;
		};
		for (size_t Index = 0; Index < Actual.size(); ++Index)
		{
			ASSERT_THAT(IsTrue(Actual[Index].Name == Expected[Index].Name));
			ASSERT_THAT(IsTrue(Actual[Index].Bytes == Expected[Index].Bytes));
			for (size_t Other = 0; Other < Index; ++Other)
			{
				ASSERT_THAT(IsFalse(
					Actual[Index].Name == Actual[Other].Name
					|| Actual[Index].Bytes == Actual[Other].Bytes));
			}
			for (const unsigned char Byte : Actual[Index].Name)
			{
				Mix(Byte);
			}
			Mix(0);
			for (const unsigned char Byte : Actual[Index].Bytes)
			{
				Mix(Byte);
			}
			Mix(0);
		}
		ASSERT_THAT(AreEqual(0x5b75772962a8f9f5ull, Fingerprint));
	}

	TEST_METHOD(MalformedUtf8FamiliesAdvanceExactly)
	{
		const std::vector<FMalformedUtf8Case> Cases =
			FNativeEngineTokenizerTest::GenerateMalformedUtf8Cases();
		for (const FMalformedUtf8Case& Case : Cases)
		{
			FLexExpectation Expected;
			for (size_t Offset = 0; Offset < Case.Bytes.size(); ++Offset)
			{
				if (static_cast<uint8>(Case.Bytes[Offset]) == static_cast<uint8>(' '))
				{
					continue;
				}
				Expected.Tokens.push_back(FExpectedToken{
					AS_NAMESPACE_QUALIFIER asETokenKind::Invalid,
					static_cast<asUINT>(Offset),
					static_cast<asUINT>(Offset + 1),
				});
				Expected.Diagnostics.push_back(FExpectedLexDiagnostic{
					AS_NAMESPACE_QUALIFIER asELexDiagnosticID::InvalidUtf8,
					static_cast<asUINT>(Offset),
					static_cast<asUINT>(Offset + 1),
				});
			}
			Expected.Tokens.push_back(FExpectedToken{
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
				static_cast<asUINT>(Case.Bytes.size()),
				static_cast<asUINT>(Case.Bytes.size()),
			});

			FNativeEngineTokenizerTest Lexer =
				FNativeEngineTokenizerTest::FromUtf8Bytes(Case.Bytes);
			if (!Lexer.Check(Assert, Expected, Case.Name))
			{
				return;
			}
			const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
			const std::span<const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Diagnostics =
				Lexer.GetDiagnostics();
			ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, Tokens[0].Kind));
			ASSERT_THAT(AreEqual(0u, Tokens[0].Range.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(1u, Tokens[0].Range.End.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::InvalidUtf8),
				Diagnostics[0].DiagnosticID));
			ASSERT_THAT(AreEqual(0u, Diagnostics[0].PrimaryRange.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(1u, Diagnostics[0].PrimaryRange.End.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Case.Bytes.size()),
				Lexer.GetRepeatedEndOfFile().Range.Begin.ByteOffset));
		}
	}

	TEST_METHOD(EmbeddedNulAndUnknownInputStayDistinct)
	{
		const std::string Source("a\x00$b", 4);
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes(Source);
		if (!Lexer.Check(
			Assert,
			FLexExpectation{
				{
					{AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, 0, 1},
					{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 1, 2},
					{AS_NAMESPACE_QUALIFIER asETokenKind::Invalid, 2, 3},
					{AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, 3, 4},
					{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 4, 4},
				},
				{
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::EmbeddedNul, 1, 2},
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnexpectedCharacter, 2, 3},
				},
			},
			"embedded NUL versus unknown input"))
		{
			return;
		}
	}

	TEST_METHOD(ValidUnicodeNonIdentifierUsesUnexpectedCharacter)
	{
		const std::string Source("\xC2\xA2", 2);
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes(Source);
		Lexer.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode));
		if (!Lexer.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Invalid,
						0,
						2,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
							| AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode},
					{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 2, 2},
				},
				{
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnexpectedCharacter, 0, 2},
				},
			},
			"valid nonidentifier Unicode"))
		{
			return;
		}
	}

	TEST_METHOD(UnicodeContinuationRespectsFrozenPolicy)
	{
		FNativeEngineTokenizerTest Unicode =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("name\u03c0"));
		Unicode.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AllowUnicode));
		if (!Unicode.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
						0,
						6,
						AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode},
					{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 6, 6},
				},
				{},
			},
			"Unicode continuation allowed"))
		{
			return;
		}

		FNativeEngineTokenizerTest Ascii =
			FNativeEngineTokenizerTest::FromText(TEXTVIEW("name\u03c0"));
		if (!Ascii.Check(
			Assert,
			FLexExpectation{
				{
					{AS_NAMESPACE_QUALIFIER asETokenKind::Identifier, 0, 4},
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Invalid,
						4,
						6,
						AS_NAMESPACE_QUALIFIER asETokenFlags::ContainsUnicode},
					{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 6, 6},
				},
				{
					{AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnicodeIdentifierDisallowed, 4, 6},
				},
			},
			"Unicode continuation disallowed"))
		{
			return;
		}
	}

	TEST_METHOD(QuotedStringsTerminateOrDiagnoseExactly)
	{
		struct FCase
		{
			std::string Name;
			std::string Source;
			bool bTerminated;
		};
		const std::vector<FCase> Cases = {
			{"escaped-double", "\"a\\\"b\"", true},
			{"escaped-single", "'a\\'b'", true},
			{"unterminated-regular", "\"abc", false},
			{"unterminated-triple", "\"\"\"abc", false},
			{"dangling-escape", std::string{'\"', '\\'}, false},
		};
		for (const FCase& Case : Cases)
		{
			FLexExpectation Expected;
			Expected.Tokens.push_back(FExpectedToken{
				AS_NAMESPACE_QUALIFIER asETokenKind::StringLiteral,
				0,
				static_cast<asUINT>(Case.Source.size()),
				Case.bTerminated
					? AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
					: AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
						| AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated,
				Case.bTerminated
					? AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated
					: AS_NAMESPACE_QUALIFIER asETokenFlags::None,
			});
			Expected.Tokens.push_back({
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
				static_cast<asUINT>(Case.Source.size()),
				static_cast<asUINT>(Case.Source.size()),
			});
			if (!Case.bTerminated)
			{
				Expected.Diagnostics.push_back({
					AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnterminatedString,
					0,
					static_cast<asUINT>(Case.Source.size()),
				});
			}

			FNativeEngineTokenizerTest Lexer =
				FNativeEngineTokenizerTest::FromUtf8Bytes(Case.Source);
			if (!Lexer.Check(Assert, Expected, Case.Name))
			{
				return;
			}
		}
	}

	TEST_METHOD(CommentAtBufferEndTerminatesExactly)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes("//");
		Lexer.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		if (!Lexer.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::LineComment,
						0,
						2,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine,
						AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated},
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
						2,
						2,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
							| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace},
				},
				{},
			},
			"line comment at EOF"))
		{
			return;
		}
	}

	TEST_METHOD(TriviaAndCommentTransitionsPreserveFlags)
	{
		constexpr std::string_view Source = "x /*a\r\nb*/ y\r\n//c\r\nz";
		FNativeEngineTokenizerTest Skipped = FNativeEngineTokenizerTest::FromUtf8Bytes(Source);
		if (!Skipped.CheckKinds(
			Assert,
			{
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
			},
			"skip trivia transitions"))
		{
			return;
		}

		FNativeEngineTokenizerTest Retained = FNativeEngineTokenizerTest::FromUtf8Bytes(Source);
		Retained.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		if (!Retained.CheckKinds(
			Assert,
			{
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
				AS_NAMESPACE_QUALIFIER asETokenKind::BlockComment,
				AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
				AS_NAMESPACE_QUALIFIER asETokenKind::LineComment,
				AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
				AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
				AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
			},
			"retain trivia transitions"))
		{
			return;
		}

		std::vector<AS_NAMESPACE_QUALIFIER asCToken> RetainedNonTrivia;
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Retained.GetTokens())
		{
			if (!AS_NAMESPACE_QUALIFIER asIsTrivia(Token.Kind))
			{
				RetainedNonTrivia.push_back(Token);
			}
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::BlockComment
				|| Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::LineComment)
			{
				ASSERT_THAT(IsFalse(EnumHasAnyFlags(
					Token.Flags,
					AS_NAMESPACE_QUALIFIER asETokenFlags::Unterminated)));
			}
		}
		ASSERT_THAT(IsTrue(SameProjection(Skipped.GetTokens(), RetainedNonTrivia)));
		ASSERT_THAT(IsTrue(Retained.GetDiagnostics().empty()));
		ASSERT_THAT(IsTrue(EnumHasAllFlags(
			Skipped.GetTokens()[1].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
				| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace)));
		ASSERT_THAT(IsTrue(EnumHasAllFlags(
			Skipped.GetTokens()[2].Flags,
			AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
				| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace)));

		constexpr std::string_view DirectiveSource = "//c\n#x";
		FNativeEngineTokenizerTest DirectiveRetained =
			FNativeEngineTokenizerTest::FromUtf8Bytes(DirectiveSource);
		DirectiveRetained.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		FNativeEngineTokenizerTest DirectiveRaw =
			FNativeEngineTokenizerTest::FromUtf8Bytes(DirectiveSource);
		DirectiveRaw.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RawDirective));
		const AS_NAMESPACE_QUALIFIER asETokenKind DirectiveKinds[] = {
			AS_NAMESPACE_QUALIFIER asETokenKind::LineComment,
			AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
			AS_NAMESPACE_QUALIFIER asETokenKind::Hash,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
		};
		if (!DirectiveRetained.CheckKinds(Assert, DirectiveKinds, "retained directive shape")
			|| !DirectiveRaw.CheckKinds(Assert, DirectiveKinds, "raw directive shape"))
		{
			return;
		}
		ASSERT_THAT(IsTrue(SameProjection(
			DirectiveRetained.GetTokens(),
			DirectiveRaw.GetTokens())));
		ASSERT_THAT(IsTrue(DirectiveRetained.GetDiagnostics().empty()));
		ASSERT_THAT(IsTrue(DirectiveRaw.GetDiagnostics().empty()));
	}

	TEST_METHOD(Utf8BomIsWhitespace)
	{
		const std::string Bom("\xEF\xBB\xBF", 3);
		FNativeEngineTokenizerTest Retained = FNativeEngineTokenizerTest::FromUtf8Bytes(Bom);
		Retained.WithOptions(AS_NAMESPACE_QUALIFIER asSLexOptions(
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
			AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia));
		if (!Retained.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::Whitespace,
						0,
						3,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine},
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
						3,
						3,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
							| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace},
				},
				{},
			},
			"UTF-8 BOM retained as whitespace"))
		{
			return;
		}

		std::string BomClass = Bom;
		BomClass += "class";
		FNativeEngineTokenizerTest Skipped =
			FNativeEngineTokenizerTest::FromUtf8Bytes(BomClass);
		if (!Skipped.Check(
			Assert,
			FLexExpectation{
				{
					FExpectedToken{
						AS_NAMESPACE_QUALIFIER asETokenKind::KwClass,
						3,
						8,
						AS_NAMESPACE_QUALIFIER asETokenFlags::StartOfLine
							| AS_NAMESPACE_QUALIFIER asETokenFlags::LeadingSpace},
					{AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile, 8, 8},
				},
				{},
			},
			"UTF-8 BOM skipped before keyword"))
		{
			return;
		}
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
