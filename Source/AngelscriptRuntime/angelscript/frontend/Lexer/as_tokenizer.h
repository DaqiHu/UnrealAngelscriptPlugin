/*
   Maintained Unreal AngelScript fork — pull-based frontend tokenizer.
*/

#pragma once

#include "frontend/Basic/as_character_stream.h"
#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Lexer/as_frontend_options.h"
#include "frontend/Basic/as_identifier_table.h"

#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

enum class asELexDiagnosticID : asUINT
	{
		InvalidUtf8 = 1001,
		UnicodeIdentifierDisallowed = 1002,
		EmbeddedNul = 1003,
		UnexpectedCharacter = 1004,
		UnterminatedString = 1005,
		UnterminatedBlockComment = 1006,
	};

	class ANGELSCRIPTRUNTIME_API asCTokenizer
	{
	public:
		asCTokenizer(
			const asCSourceManager& InSourceManager,
			const asCSourceRange& SourceRange,
			const asSLexOptions& InOptions,
			asCIdentifierTable& InIdentifierTable,
			asCDiagnosticsEngine& InDiagnostics);
		~asCTokenizer();

		asCTokenizer(const asCTokenizer&) = delete;
		asCTokenizer& operator=(const asCTokenizer&) = delete;

		bool IsValid() const { return Stream.IsValid(); }
		bool Lex(asCToken& OutToken);
		bool FlushDiagnostics();
		bool HasErrors() const { return bHasErrors; }
		asCDiagnostic Diag(const asCSourceRange& Range, asUINT ID);
		asUINT GetCurrentByteOffset() const { return Stream.GetByteOffset(); }

	private:
		asETokenFlags GetCurrentFlags() const;
		void SetToken(
			asCToken& OutToken,
			asETokenKind Kind,
			asUINT BeginByteOffset,
			asUINT EndByteOffset,
			asETokenFlags Flags,
			const asCIdentifierInfo* Identifier = nullptr) const;
		bool LexWhitespace(asCToken& OutToken, asETokenFlags Flags);
		bool LexComment(asCToken& OutToken, asETokenFlags Flags);
		bool LexIdentifier(asCToken& OutToken, asETokenFlags Flags);
		bool LexNumber(asCToken& OutToken, asETokenFlags Flags);
		bool LexString(asCToken& OutToken, asETokenFlags Flags);
		bool LexPunctuation(asCToken& OutToken, asETokenFlags Flags);
		bool LexUnicodeOrInvalid(asCToken& OutToken, asETokenFlags Flags);
		bool EmitInvalid(
			asCToken& OutToken,
			asETokenFlags Flags,
			asUINT ByteCount,
			asELexDiagnosticID DiagnosticID);
		void ReportDiagnostic(asELexDiagnosticID DiagnosticID, const asCSourceRange& Range);

		asCSourceManager SourceManager;
		asCCharacterStream Stream;
		const asSLexOptions Options;
		asCIdentifierTable& IdentifierTable;
		asCDiagnosticsEngine* Diagnostics = nullptr;
		TUniquePtr<asCDiagnosticFragment> DiagnosticFragment;
		bool bAtStartOfLine = true;
		bool bHasLeadingSpace = false;
		bool bDiagnosticsFlushed = false;
		bool bHasErrors = false;
	};

END_AS_NAMESPACE
