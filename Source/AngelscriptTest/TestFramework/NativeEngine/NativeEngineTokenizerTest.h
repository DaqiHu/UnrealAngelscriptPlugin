#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/Utf8String.h"
#include "frontend/Basic/as_source_manager.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "Templates/SharedPointer.h"
#include "unreal/AngelscriptSource.h"

#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct FNoDiscardAsserter;

namespace LexerTest
{
	inline TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> MakeOwnedSource(
		FStringView Path,
		std::string_view Bytes)
	{
		FUtf8String Body;
		if (!Bytes.empty())
		{
			Body.AppendChars(
				reinterpret_cast<const UTF8CHAR*>(Bytes.data()),
				static_cast<int32>(Bytes.size()));
		}
		return MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(FString(Path), MoveTemp(Body));
	}

	inline AS_NAMESPACE_QUALIFIER asCSourceManager MakeOwnedSourceManager(
		FStringView Path,
		std::string_view Bytes,
		AS_NAMESPACE_QUALIFIER asCSourceFileID* OutFileID = nullptr)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(MakeOwnedSource(Path, Bytes));
		AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Sources);
		if (OutFileID)
		{
			*OutFileID = AS_NAMESPACE_QUALIFIER asCSourceFileID(Manager.IsReady() ? 1 : 0);
		}
		return Manager;
	}

	struct FExpectedToken
	{
		AS_NAMESPACE_QUALIFIER asETokenKind Kind;
		asUINT Begin;
		asUINT End;
		AS_NAMESPACE_QUALIFIER asETokenFlags RequiredFlags =
			AS_NAMESPACE_QUALIFIER asETokenFlags::None;
		AS_NAMESPACE_QUALIFIER asETokenFlags ForbiddenFlags =
			AS_NAMESPACE_QUALIFIER asETokenFlags::None;
	};

	struct FExpectedLexDiagnostic
	{
		AS_NAMESPACE_QUALIFIER asELexDiagnosticID ID;
		asUINT Begin;
		asUINT End;
	};

	struct FLexExpectation
	{
		std::vector<FExpectedToken> Tokens;
		std::vector<FExpectedLexDiagnostic> Diagnostics;
	};

	struct FMalformedUtf8Case
	{
		std::string Name;
		std::string Bytes;
	};

	class FNativeEngineTokenizerTest
	{
	public:
		static FNativeEngineTokenizerTest FromText(FStringView Text);
		static FNativeEngineTokenizerTest FromUtf8Bytes(std::string_view Utf8Bytes);

		~FNativeEngineTokenizerTest();
		FNativeEngineTokenizerTest(FNativeEngineTokenizerTest&& Other);
		FNativeEngineTokenizerTest& operator=(FNativeEngineTokenizerTest&& Other);
		FNativeEngineTokenizerTest(const FNativeEngineTokenizerTest&) = delete;
		FNativeEngineTokenizerTest& operator=(const FNativeEngineTokenizerTest&) = delete;

		FNativeEngineTokenizerTest& WithOptions(
			const AS_NAMESPACE_QUALIFIER asSLexOptions& Options);

		bool CheckKinds(
			FNoDiscardAsserter& Assert,
			std::span<const AS_NAMESPACE_QUALIFIER asETokenKind> ExpectedKinds,
			std::string_view CaseName = {});
		bool CheckKinds(
			FNoDiscardAsserter& Assert,
			std::initializer_list<AS_NAMESPACE_QUALIFIER asETokenKind> ExpectedKinds,
			std::string_view CaseName = {});
		bool Check(
			FNoDiscardAsserter& Assert,
			const FLexExpectation& Expected,
			std::string_view CaseName = {});

		std::string_view GetSourceBytes() const;
		std::span<const AS_NAMESPACE_QUALIFIER asCToken> GetTokens();
		std::span<const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> GetDiagnostics();
		const AS_NAMESPACE_QUALIFIER asCToken& GetRepeatedEndOfFile();
		std::string Describe();

		static std::vector<FMalformedUtf8Case> GenerateMalformedUtf8Cases();

	private:
		struct FRunState;

		explicit FNativeEngineTokenizerTest(std::string InSourceBytes);
		void RunOnce();
		bool ValidateRun(std::string& OutReason);
		bool Fail(
			FNoDiscardAsserter& Assert,
			std::string_view CaseName,
			std::string_view Reason);

		std::string SourceBytes;
		AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy UnicodePolicy =
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly;
		AS_NAMESPACE_QUALIFIER asETriviaMode TriviaMode =
			AS_NAMESPACE_QUALIFIER asETriviaMode::SkipTrivia;
		std::unique_ptr<FRunState> RunState;
	};
}
