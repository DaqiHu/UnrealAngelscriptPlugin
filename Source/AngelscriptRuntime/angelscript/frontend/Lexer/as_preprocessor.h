/*
   Maintained Unreal AngelScript fork — deterministic conditional token routing.
*/

#pragma once

#include "frontend/Basic/as_diagnostics.h"
#include "frontend/Lexer/as_directive_tree.h"
#include "frontend/Lexer/as_preprocessing_record.h"
#include "frontend/Lexer/as_token.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE

class asCIdentifierTable;

	enum class asEPreprocessorDiagnosticID : asUINT
	{
		UnknownFlag = 2001,
		MalformedCondition = 2002,
		UnexpectedElif = 2003,
		UnexpectedElse = 2004,
		UnexpectedEndif = 2005,
		ElifAfterElse = 2006,
		DuplicateElse = 2007,
		MissingEndif = 2008,
		UnsupportedDirective = 2009,
		InvalidRestrictionOperation = 2010,
		InvalidRestrictionPolicy = 2011,
		MissingRestrictionPattern = 2012,
		ExtraRestrictionArgument = 2013,
		UnsupportedInclude = 2014,
	};

	struct ANGELSCRIPTRUNTIME_API asSPreprocessorFlag
	{
		FString Name;
		bool bEnabled = false;
	};

	class ANGELSCRIPTRUNTIME_API asCPreprocessorConfiguration
	{
	public:
		explicit asCPreprocessorConfiguration(TMap<FString, bool> InFlags = {});

		asCPreprocessorConfiguration(const asCPreprocessorConfiguration&) = default;
		asCPreprocessorConfiguration(asCPreprocessorConfiguration&&) = default;
		asCPreprocessorConfiguration& operator=(const asCPreprocessorConfiguration&) = delete;
		asCPreprocessorConfiguration& operator=(asCPreprocessorConfiguration&&) = delete;

		bool ContainsFlag(FStringView Name) const;
		bool TryGetFlag(FStringView Name, bool& OutEnabled) const;
		uint64 GetDigest() const { return ConfigurationDigest; }
		TConstArrayView<asSPreprocessorFlag> GetFlags() const { return MakeArrayView(Flags); }

	private:
		TArray<asSPreprocessorFlag> Flags;
		uint64 ConfigurationDigest = 0;
	};

	struct ANGELSCRIPTRUNTIME_API asSDirectiveProcessResult
	{
		bool IsValid() const { return bValid; }

		bool bValid = false;
		uint64 SnapshotID = 0;
		uint64 ConfigurationDigest = 0;
		asCSourceManager Sources;
		// Token identifier pointers remain valid independently of the caller's lexer.
		TSharedPtr<asCIdentifierTable, ESPMode::ThreadSafe> IdentifierOwner;
		TArray<asCToken> RawTokens;
		TArray<asCToken> ActiveTokens;
		asCDirectiveTree DirectiveTree;
		asCPreprocessingRecord PreprocessingRecord;
	};

	class ANGELSCRIPTRUNTIME_API asCPreprocessor
	{
	public:
		asCPreprocessor(
			const asCSourceManager& InSourceManager,
			const asCPreprocessorConfiguration& InConfiguration,
			asCDiagnosticsEngine& InDiagnostics);

		asSDirectiveProcessResult Process(TConstArrayView<asCToken> RawTokens) const;

	private:
		asCSourceManager SourceManager;
		asCPreprocessorConfiguration Configuration;
		asCDiagnosticsEngine* Diagnostics = nullptr;
	};

END_AS_NAMESPACE
