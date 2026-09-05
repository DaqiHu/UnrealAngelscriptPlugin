/*
   Maintained Unreal AngelScript fork — pull-based frontend tokenizer.
*/

#include "frontend/as_tokenizer.h"

BEGIN_AS_NAMESPACE

namespace
	{
		struct asSUtf8DecodeResult
		{
			uint32 CodePoint = 0;
			asUINT ByteCount = 1;
			bool bValid = false;
		};

		asSUtf8DecodeResult DecodeUtf8(const uint8* Bytes, asUINT Remaining)
		{
			asSUtf8DecodeResult Result;
			if (Remaining == 0) { return Result; }
			const uint8 First = Bytes[0];
			if (First < 0x80)
			{
				Result.CodePoint = First;
				Result.ByteCount = 1;
				Result.bValid = true;
				return Result;
			}

			asUINT Expected = 0;
			uint32 CodePoint = 0;
			uint32 Minimum = 0;
			if ((First & 0xE0) == 0xC0)
			{
				Expected = 2;
				CodePoint = First & 0x1F;
				Minimum = 0x80;
			}
			else if ((First & 0xF0) == 0xE0)
			{
				Expected = 3;
				CodePoint = First & 0x0F;
				Minimum = 0x800;
			}
			else if ((First & 0xF8) == 0xF0)
			{
				Expected = 4;
				CodePoint = First & 0x07;
				Minimum = 0x10000;
			}
			else
			{
				return Result;
			}
			if (Remaining < Expected) { return Result; }
			for (asUINT Index = 1; Index < Expected; ++Index)
			{
				if ((Bytes[Index] & 0xC0) != 0x80) { return Result; }
				CodePoint = (CodePoint << 6) | (Bytes[Index] & 0x3F);
			}
			if (CodePoint < Minimum
				|| CodePoint > 0x10FFFF
				|| (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
			{
				return Result;
			}
			Result.CodePoint = CodePoint;
			Result.ByteCount = Expected;
			Result.bValid = true;
			return Result;
		}

		bool IsAsciiIdentifierStart(uint8 Byte)
		{
			return (Byte >= 'a' && Byte <= 'z')
				|| (Byte >= 'A' && Byte <= 'Z')
				|| Byte == '_';
		}

		bool IsAsciiIdentifierContinue(uint8 Byte)
		{
			return IsAsciiIdentifierStart(Byte) || (Byte >= '0' && Byte <= '9');
		}

		bool IsAsciiDigit(uint8 Byte)
		{
			return Byte >= '0' && Byte <= '9';
		}

		bool IsUnicodeIdentifierStart(uint32 CodePoint)
		{
			return CodePoint > 0xFFFF
				|| FChar::IsAlpha(static_cast<TCHAR>(CodePoint));
		}

		bool IsUnicodeIdentifierContinue(uint32 CodePoint)
		{
			return CodePoint > 0xFFFF
				|| FChar::IsAlnum(static_cast<TCHAR>(CodePoint));
		}
	}

	asCTokenizer::asCTokenizer(
		const asCSourceManager& InSourceManager,
		const asCSourceRange& SourceRange,
		const asSLexOptions& InOptions,
		asCIdentifierTable& InIdentifierTable,
		asCDiagnosticsEngine& InDiagnostics)
		: SourceManager(InSourceManager)
		, Options(InOptions)
		, IdentifierTable(InIdentifierTable)
		, Diagnostics(&InDiagnostics)
		, DiagnosticFragment(MakeUnique<asCDiagnosticFragment>(
			InDiagnostics.CreateFragment(TEXT("lexer"))))
	{
		asCCharacterStream::TryCreate(SourceManager, SourceRange, Stream);
	}

	asCTokenizer::~asCTokenizer() = default;

	asETokenFlags asCTokenizer::GetCurrentFlags() const
	{
		asETokenFlags Flags = asETokenFlags::None;
		if (bAtStartOfLine) { Flags |= asETokenFlags::StartOfLine; }
		if (bHasLeadingSpace) { Flags |= asETokenFlags::LeadingSpace; }
		return Flags;
	}

	void asCTokenizer::SetToken(
		asCToken& OutToken,
		asETokenKind Kind,
		asUINT BeginByteOffset,
		asUINT EndByteOffset,
		asETokenFlags Flags,
		const asCIdentifierInfo* Identifier) const
	{
		OutToken = asCToken();
		OutToken.Kind = Kind;
		OutToken.Flags = Flags;
		OutToken.Range = SourceManager.MakeRange(
			Stream.GetBoundRange().Begin.FileID,
			BeginByteOffset,
			EndByteOffset);
		OutToken.Identifier = Identifier;
	}

	bool asCTokenizer::Lex(asCToken& OutToken)
	{
		OutToken = asCToken();
		if (!IsValid())
		{
			return false;
		}

		for (;;)
		{
			if (Stream.IsAtEnd())
			{
				const asUINT Offset = Stream.GetByteOffset();
				SetToken(OutToken, asETokenKind::EndOfFile, Offset, Offset, GetCurrentFlags());
				return true;
			}

			const asETokenFlags Flags = GetCurrentFlags();
			const uint8 Byte = Stream.Peek();
			if (Byte == ' ' || Byte == '\t' || Byte == '\r' || Byte == '\n')
			{
				if (!LexWhitespace(OutToken, Flags)) { return false; }
				if (Options.GetTriviaMode() == asETriviaMode::SkipTrivia) { continue; }
				return true;
			}
			if (Byte == '/' && (Stream.Peek(1) == '/' || Stream.Peek(1) == '*'))
			{
				if (!LexComment(OutToken, Flags)) { return false; }
				if (Options.GetTriviaMode() == asETriviaMode::SkipTrivia) { continue; }
				return true;
			}
			if (IsAsciiIdentifierStart(Byte)) { return LexIdentifier(OutToken, Flags); }
			if (Byte >= 0x80) { return LexUnicodeOrInvalid(OutToken, Flags); }
			if (IsAsciiDigit(Byte) || (Byte == '.' && IsAsciiDigit(Stream.Peek(1)))) { return LexNumber(OutToken, Flags); }
			if (Byte == '"' || Byte == '\'') { return LexString(OutToken, Flags); }
			if (LexPunctuation(OutToken, Flags)) { return true; }
			return EmitInvalid(
				OutToken,
				Flags,
				1,
				Byte == 0 ? asELexDiagnosticID::EmbeddedNul : asELexDiagnosticID::UnexpectedCharacter);
		}
	}

	bool asCTokenizer::LexWhitespace(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		bool bSawNewline = false;
		while (!Stream.IsAtEnd())
		{
			const uint8 Byte = Stream.Peek();
			if (Byte != ' ' && Byte != '\t' && Byte != '\r' && Byte != '\n') { break; }
			bSawNewline |= Byte == '\n';
			Stream.Advance();
		}
		SetToken(OutToken, asETokenKind::Whitespace, Begin, Stream.GetByteOffset(), Flags);
		bHasLeadingSpace = true;
		if (bSawNewline) { bAtStartOfLine = true; }
		return true;
	}

	bool asCTokenizer::LexComment(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		const bool bLineComment = Stream.Peek(1) == '/';
		Stream.Advance(2);
		bool bSawNewline = false;
		bool bTerminated = bLineComment;
		while (!Stream.IsAtEnd())
		{
			if (bLineComment)
			{
				if (Stream.Peek() == '\n') { break; }
				Stream.Advance();
				continue;
			}
			if (Stream.Peek() == '*' && Stream.Peek(1) == '/')
			{
				Stream.Advance(2);
				bTerminated = true;
				break;
			}
			bSawNewline |= Stream.Peek() == '\n';
			Stream.Advance();
		}
		SetToken(
			OutToken,
			bLineComment ? asETokenKind::LineComment : asETokenKind::BlockComment,
			Begin,
			Stream.GetByteOffset(),
			bTerminated ? Flags : Flags | asETokenFlags::Unterminated);
		if (!bTerminated)
		{
			ReportDiagnostic(asELexDiagnosticID::UnterminatedBlockComment, OutToken.Range);
		}
		bHasLeadingSpace = true;
		if (bSawNewline) { bAtStartOfLine = true; }
		return true;
	}

	bool asCTokenizer::LexIdentifier(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		bool bContainsUnicode = false;
		while (!Stream.IsAtEnd())
		{
			const uint8 Byte = Stream.Peek();
			if (IsAsciiIdentifierContinue(Byte))
			{
				Stream.Advance();
				continue;
			}
			if (Byte < 0x80 || !Options.AllowUnicodeIdentifiers()) { break; }
			const asSUtf8DecodeResult Decoded = DecodeUtf8(
				Stream.GetCurrentPointer(),
				Stream.GetRemainingByteCount());
			if (!Decoded.bValid || !IsUnicodeIdentifierContinue(Decoded.CodePoint)) { break; }
			Stream.Advance(Decoded.ByteCount);
			bContainsUnicode = true;
		}
		const asUINT End = Stream.GetByteOffset();
		TConstArrayView<uint8> Spelling;
		if (!SourceManager.TryGetBytes(
			SourceManager.MakeRange(Stream.GetBoundRange().Begin.FileID, Begin, End),
			Spelling))
		{
			return false;
		}
		const asCIdentifierInfo* Identifier = IdentifierTable.Intern(Spelling);
		if (Identifier == nullptr) { return false; }
		SetToken(
			OutToken,
			Identifier->GetTokenKind(),
			Begin,
			End,
			bContainsUnicode ? Flags | asETokenFlags::ContainsUnicode : Flags,
			Identifier);
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	bool asCTokenizer::LexNumber(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		bool bCanTakeExponentSign = false;
		const bool bBased = Stream.Peek() == '0' && (Stream.Peek(1) == 'x' || Stream.Peek(1) == 'X'
			|| Stream.Peek(1) == 'b' || Stream.Peek(1) == 'B' || Stream.Peek(1) == 'o' || Stream.Peek(1) == 'O' || Stream.Peek(1) == 'd' || Stream.Peek(1) == 'D');
		while (!Stream.IsAtEnd())
		{
			const uint8 Byte = Stream.Peek();
			const bool bAlpha = (Byte >= 'a' && Byte <= 'z') || (Byte >= 'A' && Byte <= 'Z');
			if (IsAsciiDigit(Byte)
				|| bAlpha
				|| Byte == 'x' || Byte == 'X' || Byte == '.' || Byte == '_'
				|| ((Byte == '+' || Byte == '-') && bCanTakeExponentSign))
			{
				bCanTakeExponentSign = !bBased && (Byte == 'e' || Byte == 'E');
				Stream.Advance();
				continue;
			}
			break;
		}
		SetToken(OutToken, asETokenKind::NumericLiteral, Begin, Stream.GetByteOffset(), Flags);
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	bool asCTokenizer::LexString(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		const uint8 Quote = Stream.Peek();
		const bool bTriple = Stream.Peek(1) == Quote && Stream.Peek(2) == Quote;
		Stream.Advance(bTriple ? 3 : 1);
		bool bEscaped = false;
		bool bTerminated = false;
		while (!Stream.IsAtEnd())
		{
			const uint8 Byte = Stream.Peek();
			if (bTriple)
			{
				if (Byte == Quote && Stream.Peek(1) == Quote && Stream.Peek(2) == Quote)
				{
					Stream.Advance(3); bTerminated = true; break;
				}
				Stream.Advance(); continue;
			}
			Stream.Advance();
			if (bEscaped) { bEscaped = false; continue; }
			if (Byte == '\\') { bEscaped = true; continue; }
			if (Byte == Quote) { bTerminated = true; break; }
			if (Byte == '\n' || Byte == '\r') { break; }
		}
		SetToken(
			OutToken,
			asETokenKind::StringLiteral,
			Begin,
			Stream.GetByteOffset(),
			bTerminated ? Flags : Flags | asETokenFlags::Unterminated);
		if (!bTerminated)
		{
			ReportDiagnostic(asELexDiagnosticID::UnterminatedString, OutToken.Range);
		}
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	bool asCTokenizer::LexUnicodeOrInvalid(asCToken& OutToken, asETokenFlags Flags)
	{
		const asSUtf8DecodeResult Decoded = DecodeUtf8(
			Stream.GetCurrentPointer(),
			Stream.GetRemainingByteCount());
		if (!Decoded.bValid)
		{
			return EmitInvalid(OutToken, Flags, 1, asELexDiagnosticID::InvalidUtf8);
		}
		if (!Options.AllowUnicodeIdentifiers())
		{
			return EmitInvalid(
				OutToken,
				Flags | asETokenFlags::ContainsUnicode,
				Decoded.ByteCount,
				asELexDiagnosticID::UnicodeIdentifierDisallowed);
		}
		if (!IsUnicodeIdentifierStart(Decoded.CodePoint))
		{
			return EmitInvalid(
				OutToken,
				Flags | asETokenFlags::ContainsUnicode,
				Decoded.ByteCount,
				asELexDiagnosticID::UnexpectedCharacter);
		}

		const asUINT Begin = Stream.GetByteOffset();
		Stream.Advance(Decoded.ByteCount);
		while (!Stream.IsAtEnd())
		{
			const uint8 Byte = Stream.Peek();
			if (IsAsciiIdentifierContinue(Byte))
			{
				Stream.Advance();
				continue;
			}
			if (Byte < 0x80) { break; }
			const asSUtf8DecodeResult Continuation = DecodeUtf8(
				Stream.GetCurrentPointer(),
				Stream.GetRemainingByteCount());
			if (!Continuation.bValid || !IsUnicodeIdentifierContinue(Continuation.CodePoint))
			{
				break;
			}
			Stream.Advance(Continuation.ByteCount);
		}

		const asUINT End = Stream.GetByteOffset();
		TConstArrayView<uint8> Spelling;
		if (!SourceManager.TryGetBytes(
			SourceManager.MakeRange(Stream.GetBoundRange().Begin.FileID, Begin, End),
			Spelling))
		{
			return false;
		}
		const asCIdentifierInfo* Identifier = IdentifierTable.Intern(Spelling);
		if (Identifier == nullptr) { return false; }
		SetToken(
			OutToken,
			Identifier->GetTokenKind(),
			Begin,
			End,
			Flags | asETokenFlags::ContainsUnicode,
			Identifier);
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	bool asCTokenizer::EmitInvalid(
		asCToken& OutToken,
		asETokenFlags Flags,
		asUINT ByteCount,
		asELexDiagnosticID DiagnosticID)
	{
		const asUINT Begin = Stream.GetByteOffset();
		if (ByteCount == 0 || !Stream.Advance(ByteCount))
		{
			return false;
		}
		SetToken(OutToken, asETokenKind::Invalid, Begin, Stream.GetByteOffset(), Flags);
		ReportDiagnostic(DiagnosticID, OutToken.Range);
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	void asCTokenizer::ReportDiagnostic(
		asELexDiagnosticID DiagnosticID,
		const asCSourceRange& Range)
	{
		if (!DiagnosticFragment.IsValid()) { return; }
		asSDiagnosticRecord Record;
		Record.DiagnosticID = static_cast<asUINT>(DiagnosticID);
		Record.Severity = asEDiagnosticSeverity::Error;
		Record.PrimaryRange = Range;
		DiagnosticFragment->Report(MoveTemp(Record));
	}

	bool asCTokenizer::LexPunctuation(asCToken& OutToken, asETokenFlags Flags)
	{
		const asUINT Begin = Stream.GetByteOffset();
		asETokenKind Kind = asETokenKind::Invalid;
		asUINT Length = 0;
		auto Match = [this](const ANSICHAR* Text)
		{
			const int32 TextLength = FCStringAnsi::Strlen(Text);
			if (Stream.GetRemainingByteCount() < static_cast<asUINT>(TextLength)) { return false; }
			return FMemory::Memcmp(Stream.GetCurrentPointer(), Text, TextLength) == 0;
		};

		for (asUINT Value = 0; Value < static_cast<asUINT>(asETokenKind::Count); ++Value)
		{
			const asETokenKind Candidate = static_cast<asETokenKind>(Value);
			const ANSICHAR* Spelling = asGetTokenSpellingAnsi(Candidate);
			const asUINT SpellingLength = static_cast<asUINT>(FCStringAnsi::Strlen(Spelling));
			if (SpellingLength > Length
				&& !asIsKeyword(Candidate)
				&& !asIsTrivia(Candidate)
				&& Match(Spelling))
			{
				Kind = Candidate;
				Length = SpellingLength;
			}
		}
		if (Length == 0) { return false; }
		Stream.Advance(Length);
		SetToken(OutToken, Kind, Begin, Stream.GetByteOffset(), Flags);
		bAtStartOfLine = false;
		bHasLeadingSpace = false;
		return true;
	}

	bool asCTokenizer::FlushDiagnostics()
	{
		if (bDiagnosticsFlushed || Diagnostics == nullptr || !DiagnosticFragment.IsValid())
		{
			return false;
		}
		bDiagnosticsFlushed = Diagnostics->Submit(MoveTemp(*DiagnosticFragment));
		DiagnosticFragment.Reset();
		return bDiagnosticsFlushed;
	}

END_AS_NAMESPACE
