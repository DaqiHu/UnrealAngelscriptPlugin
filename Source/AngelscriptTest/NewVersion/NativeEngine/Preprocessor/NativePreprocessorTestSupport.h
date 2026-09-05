#pragma once

#include "../NativeEngineTestSupport.h"

#include "frontend/as_diagnostics.h"
#include "frontend/as_frontend_options.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_preprocessor.h"
#include "frontend/as_source_manager.h"
#include "frontend/as_tokenizer.h"

namespace AngelscriptNativePreprocessorTest
{
	struct FPreprocessorRun
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager;
		AS_NAMESPACE_QUALIFIER asCIdentifierTable IdentifierTable;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics;
		AS_NAMESPACE_QUALIFIER asCSourceFileID FileID;
		TArray<AS_NAMESPACE_QUALIFIER asCToken> RawTokens;
		AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult Result;
		TArray<AS_NAMESPACE_QUALIFIER asSDiagnosticRecord> DiagnosticRecords;

		FPreprocessorRun(FStringView SourceText, TMap<FString, bool> Flags)
			: Snapshot(MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>())
			, SourceManager(Snapshot)
			, Diagnostics(SourceManager)
		{
			const AngelscriptNativeEngineTest::FSourceInput Input =
				AngelscriptNativeEngineTest::FSourceInput::FromText(
					TEXT("/Angelscript/Game/Preprocessor.as"), SourceText);
			FileID = Snapshot->AddFile(Input.GetLogicalPath(), Input.GetUtf8Bytes());
			Snapshot->Freeze();

			AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
				SourceManager,
				SourceManager.MakeRange(FileID, 0, static_cast<asUINT>(Input.GetUtf8Bytes().Num())),
				AS_NAMESPACE_QUALIFIER asSLexOptions(
					AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly,
					AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
				IdentifierTable,
				Diagnostics);
			for (;;)
			{
				AS_NAMESPACE_QUALIFIER asCToken Token;
				if (!Tokenizer.Lex(Token)) { break; }
				RawTokens.Add(Token);
				if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile) { break; }
			}
			Tokenizer.FlushDiagnostics();

			const AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration(MoveTemp(Flags));
			AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor(SourceManager, Configuration, Diagnostics);
			Result = Preprocessor.Process(MakeArrayView(RawTokens));

			AS_NAMESPACE_QUALIFIER asCCollectingDiagnosticConsumer Consumer;
			Diagnostics.EmitTo(Consumer);
			DiagnosticRecords.Append(Consumer.GetRecords());
		}

		FString GetActiveTokenProjection() const
		{
			FString Projection;
			for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Result.ActiveTokens)
			{
				if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile || AS_NAMESPACE_QUALIFIER asIsTrivia(Token.Kind))
				{
					continue;
				}
				TConstArrayView<uint8> Bytes;
				if (!SourceManager.TryGetBytes(Token.Range, Bytes)) { return TEXT("<invalid-range>"); }
				const FUTF8ToTCHAR Converted(
					reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
					Bytes.Num());
				if (!Projection.IsEmpty()) { Projection += TEXT("|"); }
				Projection.AppendChars(Converted.Get(), Converted.Length());
			}
			return Projection;
		}
	};

	inline TMap<FString, bool> MakeFlags(std::initializer_list<TPair<const TCHAR*, bool>> Values)
	{
		TMap<FString, bool> Result;
		for (const TPair<const TCHAR*, bool>& Value : Values)
		{
			Result.Add(Value.Key, Value.Value);
		}
		return Result;
	}
}
