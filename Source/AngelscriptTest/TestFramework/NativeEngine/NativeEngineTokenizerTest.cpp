#include "TestFramework/NativeEngine/NativeEngineTokenizerTest.h"

#include "Assert/NoDiscardAsserter.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Basic/as_source_manager.h"
#include "Containers/StringConv.h"
#include "Templates/SharedPointer.h"

#include <limits>

namespace LexerTest
{
	struct FNativeEngineTokenizerTest::FRunState
	{
		explicit FRunState(
			std::string_view SourceBytes,
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy UnicodePolicy,
			AS_NAMESPACE_QUALIFIER asETriviaMode TriviaMode)
			: SourceManager(MakeOwnedSourceManager(
				TEXTVIEW("/Angelscript/Test/Lexer.as"),
				SourceBytes,
				&FileID))
		{
			checkf(
				SourceBytes.size() <= static_cast<size_t>(std::numeric_limits<int32>::max()),
				TEXT("Lexer test input exceeds the checked TConstArrayView size."));
			DiagnosticsEngine =
				std::make_unique<AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine>(SourceManager);
			Tokenizer = std::make_unique<AS_NAMESPACE_QUALIFIER asCTokenizer>(
				SourceManager,
				SourceManager.MakeRange(FileID, 0, static_cast<asUINT>(SourceBytes.size())),
				AS_NAMESPACE_QUALIFIER asSLexOptions(UnicodePolicy, TriviaMode),
				IdentifierTable,
				*DiagnosticsEngine);
		}

		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager;
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		std::unique_ptr<AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine> DiagnosticsEngine;
		std::unique_ptr<AS_NAMESPACE_QUALIFIER asCTokenizer> Tokenizer;
		AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer DiagnosticConsumer;
		AS_NAMESPACE_QUALIFIER asCToken RepeatedEndOfFile;
		std::vector<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
		std::vector<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Diagnostics;
		std::string ExecutionFailure;
		bool bRepeatedEndOfFileCaptured = false;
		bool bDiagnosticsFlushed = false;
	};

	FNativeEngineTokenizerTest::FNativeEngineTokenizerTest(std::string InSourceBytes)
		: SourceBytes(std::move(InSourceBytes))
	{
	}

	FNativeEngineTokenizerTest::~FNativeEngineTokenizerTest() = default;
	FNativeEngineTokenizerTest::FNativeEngineTokenizerTest(FNativeEngineTokenizerTest&& Other) = default;
	FNativeEngineTokenizerTest& FNativeEngineTokenizerTest::operator=(FNativeEngineTokenizerTest&& Other) = default;

	FNativeEngineTokenizerTest FNativeEngineTokenizerTest::FromText(FStringView Text)
	{
		const FTCHARToUTF8 Converted(Text.GetData(), Text.Len());
		return FNativeEngineTokenizerTest(std::string(Converted.Get(), Converted.Length()));
	}

	FNativeEngineTokenizerTest FNativeEngineTokenizerTest::FromUtf8Bytes(std::string_view Utf8Bytes)
	{
		return FNativeEngineTokenizerTest(std::string(Utf8Bytes));
	}

	FNativeEngineTokenizerTest& FNativeEngineTokenizerTest::WithOptions(
		const AS_NAMESPACE_QUALIFIER asSLexOptions& Options)
	{
		checkf(RunState == nullptr, TEXT("Lexer test options must be configured before observation."));
		UnicodePolicy = Options.GetUnicodeIdentifierPolicy();
		TriviaMode = Options.GetTriviaMode();
		return *this;
	}

	bool FNativeEngineTokenizerTest::CheckKinds(
		FNoDiscardAsserter& Assert,
		std::span<const AS_NAMESPACE_QUALIFIER asETokenKind> ExpectedKinds,
		std::string_view CaseName)
	{
		std::string Reason;
		if (!ValidateRun(Reason))
		{
			return Fail(Assert, CaseName, Reason);
		}

		if (RunState->Tokens.size() != ExpectedKinds.size())
		{
			Reason = "expected " + std::to_string(ExpectedKinds.size())
				+ " token(s), observed " + std::to_string(RunState->Tokens.size());
			return Fail(Assert, CaseName, Reason);
		}

		for (size_t Index = 0; Index < ExpectedKinds.size(); ++Index)
		{
			if (RunState->Tokens[Index].Kind != ExpectedKinds[Index])
			{
				Reason = "token " + std::to_string(Index) + " expected kind ";
				const FTCHARToUTF8 ExpectedName(
					AS_NAMESPACE_QUALIFIER asGetTokenKindName(ExpectedKinds[Index]));
				Reason.append(ExpectedName.Get(), ExpectedName.Length());
				Reason += ", observed ";
				const FTCHARToUTF8 ActualName(
					AS_NAMESPACE_QUALIFIER asGetTokenKindName(RunState->Tokens[Index].Kind));
				Reason.append(ActualName.Get(), ActualName.Length());
				return Fail(Assert, CaseName, Reason);
			}
		}
		return true;
	}

	bool FNativeEngineTokenizerTest::CheckKinds(
		FNoDiscardAsserter& Assert,
		std::initializer_list<AS_NAMESPACE_QUALIFIER asETokenKind> ExpectedKinds,
		std::string_view CaseName)
	{
		return CheckKinds(
			Assert,
			std::span<const AS_NAMESPACE_QUALIFIER asETokenKind>(ExpectedKinds.begin(), ExpectedKinds.size()),
			CaseName);
	}

	bool FNativeEngineTokenizerTest::Check(
		FNoDiscardAsserter& Assert,
		const FLexExpectation& Expected,
		std::string_view CaseName)
	{
		std::string Reason;
		if (!ValidateRun(Reason))
		{
			return Fail(Assert, CaseName, Reason);
		}
		if (RunState->Tokens.size() != Expected.Tokens.size())
		{
			Reason = "expected " + std::to_string(Expected.Tokens.size())
				+ " token(s), observed " + std::to_string(RunState->Tokens.size());
			return Fail(Assert, CaseName, Reason);
		}

		for (size_t Index = 0; Index < Expected.Tokens.size(); ++Index)
		{
			const FExpectedToken& ExpectedToken = Expected.Tokens[Index];
			const AS_NAMESPACE_QUALIFIER asCToken& ActualToken = RunState->Tokens[Index];
			if (ActualToken.Kind != ExpectedToken.Kind
				|| ActualToken.Range.Begin.ByteOffset != ExpectedToken.Begin
				|| ActualToken.Range.End.ByteOffset != ExpectedToken.End)
			{
				Reason = "token " + std::to_string(Index) + " kind/range mismatch";
				return Fail(Assert, CaseName, Reason);
			}

			const uint8 ActualFlags = static_cast<uint8>(ActualToken.Flags);
			const uint8 RequiredFlags = static_cast<uint8>(ExpectedToken.RequiredFlags);
			const uint8 ForbiddenFlags = static_cast<uint8>(ExpectedToken.ForbiddenFlags);
			if ((ActualFlags & RequiredFlags) != RequiredFlags
				|| (ActualFlags & ForbiddenFlags) != 0)
			{
				Reason = "token " + std::to_string(Index) + " flag mismatch";
				return Fail(Assert, CaseName, Reason);
			}
		}

		if (RunState->Diagnostics.size() != Expected.Diagnostics.size())
		{
			Reason = "expected " + std::to_string(Expected.Diagnostics.size())
				+ " diagnostic(s), observed " + std::to_string(RunState->Diagnostics.size());
			return Fail(Assert, CaseName, Reason);
		}
		for (size_t Index = 0; Index < Expected.Diagnostics.size(); ++Index)
		{
			const FExpectedLexDiagnostic& ExpectedDiagnostic = Expected.Diagnostics[Index];
			const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& ActualDiagnostic =
				RunState->Diagnostics[Index];
			if (ActualDiagnostic.DiagnosticID != static_cast<asUINT>(ExpectedDiagnostic.ID)
				|| ActualDiagnostic.PrimaryRange.Begin.ByteOffset != ExpectedDiagnostic.Begin
				|| ActualDiagnostic.PrimaryRange.End.ByteOffset != ExpectedDiagnostic.End)
			{
				Reason = "diagnostic " + std::to_string(Index) + " id/range mismatch";
				return Fail(Assert, CaseName, Reason);
			}
		}
		return true;
	}

	std::string_view FNativeEngineTokenizerTest::GetSourceBytes() const
	{
		return SourceBytes;
	}

	std::span<const AS_NAMESPACE_QUALIFIER asCToken> FNativeEngineTokenizerTest::GetTokens()
	{
		RunOnce();
		return RunState->Tokens;
	}

	std::span<const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> FNativeEngineTokenizerTest::GetDiagnostics()
	{
		RunOnce();
		return RunState->Diagnostics;
	}

	const AS_NAMESPACE_QUALIFIER asCToken& FNativeEngineTokenizerTest::GetRepeatedEndOfFile()
	{
		RunOnce();
		return RunState->RepeatedEndOfFile;
	}

	std::string FNativeEngineTokenizerTest::Describe()
	{
		RunOnce();
		std::string Result = "bytes=";
		constexpr char Hex[] = "0123456789ABCDEF";
		for (const unsigned char Byte : SourceBytes)
		{
			Result += Hex[Byte >> 4];
			Result += Hex[Byte & 0x0f];
		}
		Result += " tokens=[";
		for (size_t Index = 0; Index < RunState->Tokens.size(); ++Index)
		{
			if (Index != 0)
			{
				Result += ",";
			}
			const AS_NAMESPACE_QUALIFIER asCToken& Token = RunState->Tokens[Index];
			const FTCHARToUTF8 KindName(AS_NAMESPACE_QUALIFIER asGetTokenKindName(Token.Kind));
			Result.append(KindName.Get(), KindName.Length());
			Result += "@" + std::to_string(Token.Range.Begin.ByteOffset)
				+ "-" + std::to_string(Token.Range.End.ByteOffset)
				+ ":" + std::to_string(static_cast<uint8>(Token.Flags));
		}
		Result += "] diagnostics=[";
		for (size_t Index = 0; Index < RunState->Diagnostics.size(); ++Index)
		{
			if (Index != 0)
			{
				Result += ",";
			}
			const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Diagnostic =
				RunState->Diagnostics[Index];
			Result += std::to_string(Diagnostic.DiagnosticID)
				+ "@" + std::to_string(Diagnostic.PrimaryRange.Begin.ByteOffset)
				+ "-" + std::to_string(Diagnostic.PrimaryRange.End.ByteOffset);
		}
		Result += "] repeated_eof=";
		if (RunState->bRepeatedEndOfFileCaptured)
		{
			Result += std::to_string(RunState->RepeatedEndOfFile.Range.Begin.ByteOffset)
				+ "-" + std::to_string(RunState->RepeatedEndOfFile.Range.End.ByteOffset);
		}
		else
		{
			Result += "missing";
		}
		Result += RunState->bDiagnosticsFlushed ? " flush=ok" : " flush=failed";
		if (!RunState->ExecutionFailure.empty())
		{
			Result += " execution_failure=" + RunState->ExecutionFailure;
		}
		return Result;
	}

	std::vector<FMalformedUtf8Case> FNativeEngineTokenizerTest::GenerateMalformedUtf8Cases()
	{
		struct FValidSeed
		{
			asUINT Width;
			std::string Bytes;
		};
		const FValidSeed Seeds[] = {
			{2, std::string("\xC2\xA2", 2)},
			{3, std::string("\xE2\x82\xAC", 3)},
			{4, std::string("\xF0\x90\x80\x80", 4)},
		};

		std::vector<FMalformedUtf8Case> Cases;
		Cases.reserve(16);
		for (const FValidSeed& Seed : Seeds)
		{
			for (size_t PrefixLength = 1; PrefixLength < Seed.Bytes.size(); ++PrefixLength)
			{
				Cases.push_back(FMalformedUtf8Case{
					"truncated-" + std::to_string(Seed.Width) + "-" + std::to_string(PrefixLength),
					Seed.Bytes.substr(0, PrefixLength),
				});
			}
		}
		for (const FValidSeed& Seed : Seeds)
		{
			std::string Mutated = Seed.Bytes;
			Mutated[1] = ' ';
			Cases.push_back(FMalformedUtf8Case{
				"bad-continuation-" + std::to_string(Seed.Width),
				std::move(Mutated),
			});
		}
		Cases.push_back({"invalid-continuation-leader", std::string("\x80", 1)});
		Cases.push_back({"invalid-leader", std::string("\xFF", 1)});
		Cases.push_back({"overlong-2", std::string("\xC0\xAF", 2)});
		Cases.push_back({"overlong-3", std::string("\xE0\x80\xAF", 3)});
		Cases.push_back({"overlong-4", std::string("\xF0\x80\x80\xAF", 4)});
		Cases.push_back({"surrogate", std::string("\xED\xA0\x80", 3)});
		Cases.push_back({"above-unicode-max", std::string("\xF4\x90\x80\x80", 4)});
		return Cases;
	}

	void FNativeEngineTokenizerTest::RunOnce()
	{
		if (RunState != nullptr)
		{
			return;
		}

		RunState = std::make_unique<FRunState>(SourceBytes, UnicodePolicy, TriviaMode);
		if (!RunState->Tokenizer->IsValid())
		{
			RunState->ExecutionFailure = "tokenizer rejected the checked source range";
		}
		else
		{
			const size_t MaximumPrimaryPulls = SourceBytes.size() + 1;
			for (size_t PullIndex = 0; PullIndex < MaximumPrimaryPulls; ++PullIndex)
			{
				AS_NAMESPACE_QUALIFIER asCToken Token;
				if (!RunState->Tokenizer->Lex(Token))
				{
					RunState->ExecutionFailure = "Lex returned false before EOF";
					break;
				}
				RunState->Tokens.push_back(Token);
				if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
				{
					break;
				}
			}

			if (RunState->Tokens.empty()
				|| RunState->Tokens.back().Kind != AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				if (RunState->ExecutionFailure.empty())
				{
					RunState->ExecutionFailure = "bounded pulls did not reach EOF";
				}
			}
			else if (RunState->Tokenizer->Lex(RunState->RepeatedEndOfFile))
			{
				RunState->bRepeatedEndOfFileCaptured = true;
			}
			else if (RunState->ExecutionFailure.empty())
			{
				RunState->ExecutionFailure = "the repeated EOF pull returned false";
			}
		}

		RunState->bDiagnosticsFlushed = RunState->Tokenizer->FlushDiagnostics();
		RunState->DiagnosticsEngine->EmitTo(RunState->DiagnosticConsumer);
		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> Records =
			RunState->DiagnosticConsumer.GetRecords();
		RunState->Diagnostics.assign(Records.begin(), Records.end());
	}

	bool FNativeEngineTokenizerTest::ValidateRun(std::string& OutReason)
	{
		RunOnce();
		if (!RunState->ExecutionFailure.empty())
		{
			OutReason = RunState->ExecutionFailure;
			return false;
		}
		if (!RunState->bDiagnosticsFlushed)
		{
			OutReason = "diagnostics did not flush exactly once successfully";
			return false;
		}
		if (RunState->Tokens.empty())
		{
			OutReason = "primary stream is empty";
			return false;
		}

		const asUINT SourceSize = static_cast<asUINT>(SourceBytes.size());
		asUINT PreviousEnd = 0;
		for (size_t Index = 0; Index < RunState->Tokens.size(); ++Index)
		{
			const AS_NAMESPACE_QUALIFIER asCToken& Token = RunState->Tokens[Index];
			if (!Token.Range.IsOrdered()
				|| Token.Range.Begin.FileID != RunState->FileID
				|| Token.Range.Begin.ByteOffset < PreviousEnd
				|| Token.Range.End.ByteOffset > SourceSize)
			{
				OutReason = "token " + std::to_string(Index)
					+ " has a non-monotonic or out-of-bounds range";
				return false;
			}
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile)
			{
				if (Index + 1 != RunState->Tokens.size()
					|| Token.Range.Begin.ByteOffset != SourceSize
					|| Token.Range.End.ByteOffset != SourceSize)
				{
					OutReason = "primary EOF is not the sole final token at the source byte count";
					return false;
				}
			}
			else if (Token.Range.Length() == 0)
			{
				OutReason = "non-EOF token " + std::to_string(Index) + " made no progress";
				return false;
			}
			PreviousEnd = Token.Range.End.ByteOffset;
		}

		if (!RunState->bRepeatedEndOfFileCaptured
			|| RunState->RepeatedEndOfFile.Kind != AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile
			|| !(RunState->RepeatedEndOfFile.Range == RunState->Tokens.back().Range)
			|| RunState->RepeatedEndOfFile.Flags != RunState->Tokens.back().Flags)
		{
			OutReason = "repeated EOF is not stable";
			return false;
		}
		return true;
	}

	bool FNativeEngineTokenizerTest::Fail(
		FNoDiscardAsserter& Assert,
		std::string_view CaseName,
		std::string_view Reason)
	{
		std::string Message(CaseName);
		if (!Message.empty())
		{
			Message += ": ";
		}
		Message.append(Reason);
		Message += "; ";
		Message += Describe();
		const FUTF8ToTCHAR Converted(Message.data(), static_cast<int32>(Message.size()));
		Assert.Fail(FString(Converted.Length(), Converted.Get()));
		return false;
	}
}
