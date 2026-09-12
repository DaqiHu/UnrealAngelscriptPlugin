/*
   Maintained Unreal AngelScript fork — compact source-referential tokens.
*/

#pragma once

#include "frontend/Basic/as_source_location.h"

BEGIN_AS_NAMESPACE

class asCIdentifierInfo;

	enum class asETokenKind : uint16
	{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) Name,
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		Count,
	};

	enum class asETokenFlags : uint8
	{
		None = 0,
		StartOfLine = 1 << 0,
		LeadingSpace = 1 << 1,
		Unterminated = 1 << 2,
		ContainsUnicode = 1 << 3,
	};
	ENUM_CLASS_FLAGS(asETokenFlags);

	struct asCToken
	{
		asETokenKind Kind = asETokenKind::Invalid;
		asETokenFlags Flags = asETokenFlags::None;
		uint8 LiteralFlags = 0;
		uint8 Reserved = 0;
		asCSourceRange Range;
		union
		{
			const asCIdentifierInfo* Identifier = nullptr;
			uint64 RawPayload;
		};
	};

	inline const TCHAR* asGetTokenKindName(asETokenKind Kind)
	{
		switch (Kind)
		{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) case asETokenKind::Name: return TEXT(#Name);
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		default: return TEXT("Invalid");
		}
	}

	inline const TCHAR* asGetTokenSpelling(asETokenKind Kind)
	{
		switch (Kind)
		{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) case asETokenKind::Name: return TEXT(Spelling);
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		default: return TEXT("");
		}
	}

	inline const ANSICHAR* asGetTokenSpellingAnsi(asETokenKind Kind)
	{
		switch (Kind)
		{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) case asETokenKind::Name: return Spelling;
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		default: return "";
		}
	}

	inline bool asIsKeyword(asETokenKind Kind)
	{
		switch (Kind)
		{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) case asETokenKind::Name: return Keyword;
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		default: return false;
		}
	}

	inline bool asIsTrivia(asETokenKind Kind)
	{
		switch (Kind)
		{
#define AS_TOKEN(Name, Spelling, Keyword, Trivia) case asETokenKind::Name: return Trivia;
#include "frontend/Lexer/as_token_kinds.def"
#undef AS_TOKEN
		default: return false;
		}
	}

END_AS_NAMESPACE
