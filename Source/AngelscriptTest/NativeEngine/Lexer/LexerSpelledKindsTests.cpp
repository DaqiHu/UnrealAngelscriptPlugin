#include "CQTest.h"
#include "TestFramework/NativeEngine/NativeEngineTokenizerTest.h"

#include "frontend/Lexer/as_token.h"

#include <cstring>
#include <string_view>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	SpelledKinds,
	"Angelscript.UnitTest.NativeEngine.Lexer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeEngineTokenizerTest = LexerTest::FNativeEngineTokenizerTest;
	static constexpr uint64 AcceptedTokenVocabularyFnv1a = 0xa912c3f84387564eull;

public:
	TEST_METHOD(SpelledKindsRoundTrip)
	{
		asUINT NonEmptyCount = 0;
		for (asUINT Value = 0;
			Value < static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asETokenKind::Count);
			++Value)
		{
			const AS_NAMESPACE_QUALIFIER asETokenKind Kind =
				static_cast<AS_NAMESPACE_QUALIFIER asETokenKind>(Value);
			const ANSICHAR* Spelling = AS_NAMESPACE_QUALIFIER asGetTokenSpellingAnsi(Kind);
			const std::string_view SpellingView(Spelling, std::strlen(Spelling));
			if (SpellingView.empty())
			{
				continue;
			}

			++NonEmptyCount;
			FNativeEngineTokenizerTest Lexer =
				FNativeEngineTokenizerTest::FromUtf8Bytes(SpellingView);
			if (!Lexer.CheckKinds(
				Assert,
				{Kind, AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile},
				SpellingView))
			{
				return;
			}

			const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
			ASSERT_THAT(AreEqual(0u, Tokens[0].Range.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(SpellingView.size()),
				Tokens[0].Range.End.ByteOffset));
			ASSERT_THAT(IsTrue(
				Lexer.GetSourceBytes().substr(0, Tokens[0].Range.Length()) == SpellingView));
			ASSERT_THAT(IsTrue(Lexer.GetDiagnostics().empty()));
		}
		ASSERT_THAT(AreEqual(107u, NonEmptyCount));
	}

	TEST_METHOD(AcceptedVocabularyFingerprint)
	{
		constexpr uint64 OffsetBasis = 14695981039346656037ull;
		constexpr uint64 Prime = 1099511628211ull;
		uint64 Fingerprint = OffsetBasis;
		asUINT NonEmptyCount = 0;
		for (asUINT Value = 0;
			Value < static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asETokenKind::Count);
			++Value)
		{
			const uint16 KindValue = static_cast<uint16>(Value);
			Fingerprint ^= static_cast<uint8>(KindValue & 0xffu);
			Fingerprint *= Prime;
			Fingerprint ^= static_cast<uint8>(KindValue >> 8u);
			Fingerprint *= Prime;

			const AS_NAMESPACE_QUALIFIER asETokenKind Kind =
				static_cast<AS_NAMESPACE_QUALIFIER asETokenKind>(Value);
			const ANSICHAR* Spelling = AS_NAMESPACE_QUALIFIER asGetTokenSpellingAnsi(Kind);
			if (Spelling[0] != '\0')
			{
				++NonEmptyCount;
			}
			for (const ANSICHAR* Byte = Spelling; *Byte != '\0'; ++Byte)
			{
				Fingerprint ^= static_cast<uint8>(*Byte);
				Fingerprint *= Prime;
			}
			Fingerprint ^= 0u;
			Fingerprint *= Prime;
		}

		ASSERT_THAT(AreEqual(
			115u,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asETokenKind::Count)));
		ASSERT_THAT(AreEqual(107u, NonEmptyCount));
		ASSERT_THAT(AreEqual(AcceptedTokenVocabularyFnv1a, Fingerprint));
	}

	TEST_METHOD(OverlappingSpellingsUseMaximalMunch)
	{
		struct FCase
		{
			std::string_view Source;
			AS_NAMESPACE_QUALIFIER asETokenKind Kind;
		};
		constexpr FCase Cases[] = {
			{">>", AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRight},
			{"**", AS_NAMESPACE_QUALIFIER asETokenKind::Power},
			{">>>", AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRightArithmetic},
			{">>=", AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRightEqual},
			{">>>=", AS_NAMESPACE_QUALIFIER asETokenKind::ShiftRightArithmeticEqual},
			{"**=", AS_NAMESPACE_QUALIFIER asETokenKind::PowerEqual},
		};

		for (const FCase& Case : Cases)
		{
			FNativeEngineTokenizerTest Lexer =
				FNativeEngineTokenizerTest::FromUtf8Bytes(Case.Source);
			if (!Lexer.CheckKinds(
				Assert,
				{Case.Kind, AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile},
				Case.Source))
			{
				return;
			}
			const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
			ASSERT_THAT(AreEqual(0u, Tokens[0].Range.Begin.ByteOffset));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Case.Source.size()),
				Tokens[0].Range.End.ByteOffset));
		}
	}

	TEST_METHOD(KeywordRecognitionIsCaseSensitive)
	{
		FNativeEngineTokenizerTest Lexer = FNativeEngineTokenizerTest::FromUtf8Bytes(
			"class Class CLASS Cast cast UPROPERTY UProperty uproperty");
		const AS_NAMESPACE_QUALIFIER asETokenKind Expected[] = {
			AS_NAMESPACE_QUALIFIER asETokenKind::KwClass,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwCast,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::KwUProperty,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::Identifier,
			AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile,
		};
		if (!Lexer.CheckKinds(Assert, Expected, "keyword casing"))
		{
			return;
		}

		constexpr std::string_view Spellings[] = {
			"class", "Class", "CLASS", "Cast", "cast", "UPROPERTY", "UProperty", "uproperty"};
		const std::span<const AS_NAMESPACE_QUALIFIER asCToken> Tokens = Lexer.GetTokens();
		for (size_t Index = 0; Index < std::size(Spellings); ++Index)
		{
			ASSERT_THAT(IsTrue(Lexer.GetSourceBytes().substr(
				Tokens[Index].Range.Begin.ByteOffset,
				Tokens[Index].Range.Length()) == Spellings[Index]));
		}
		ASSERT_THAT(IsTrue(Lexer.GetDiagnostics().empty()));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
