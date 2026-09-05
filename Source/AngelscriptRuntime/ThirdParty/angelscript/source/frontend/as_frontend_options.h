/*
   Maintained Unreal AngelScript fork — immutable frontend option snapshots.
*/

#pragma once

#include "as_config.h"

BEGIN_AS_NAMESPACE

enum class asEUnicodeIdentifierPolicy : uint8
	{
		AsciiOnly,
		AllowUnicode,
	};

	enum class asETriviaMode : uint8
	{
		SkipTrivia,
		RetainTrivia,
		RawDirective,
	};

	class asSLexOptions
	{
	public:
		explicit asSLexOptions(
			asEUnicodeIdentifierPolicy InUnicodeIdentifierPolicy = asEUnicodeIdentifierPolicy::AsciiOnly,
			asETriviaMode InTriviaMode = asETriviaMode::SkipTrivia)
			: UnicodeIdentifierPolicy(InUnicodeIdentifierPolicy)
			, TriviaMode(InTriviaMode)
		{
		}

		asSLexOptions(const asSLexOptions&) = default;
		asSLexOptions(asSLexOptions&&) = default;
		asSLexOptions& operator=(const asSLexOptions&) = delete;
		asSLexOptions& operator=(asSLexOptions&&) = delete;

		bool AllowUnicodeIdentifiers() const
		{
			return UnicodeIdentifierPolicy == asEUnicodeIdentifierPolicy::AllowUnicode;
		}
		asEUnicodeIdentifierPolicy GetUnicodeIdentifierPolicy() const { return UnicodeIdentifierPolicy; }
		asETriviaMode GetTriviaMode() const { return TriviaMode; }

	private:
		const asEUnicodeIdentifierPolicy UnicodeIdentifierPolicy;
		const asETriviaMode TriviaMode;
	};

END_AS_NAMESPACE
