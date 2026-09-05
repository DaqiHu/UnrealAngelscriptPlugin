#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_identifier_table.h"
#include "frontend/as_tokenizer.h"

namespace AngelscriptNativeEngineTest
{
	struct FSourceInput
	{
		static FSourceInput FromText(FStringView InLogicalPath, FStringView InText)
		{
			FSourceInput Result;
			Result.LogicalPath = FString(InLogicalPath);

			const FTCHARToUTF8 Converted(InText.GetData(), InText.Len());
			Result.Utf8Bytes.Append(
				reinterpret_cast<const uint8*>(Converted.Get()),
				Converted.Length());
			return Result;
		}

		const FString& GetLogicalPath() const
		{
			return LogicalPath;
		}

		TConstArrayView<uint8> GetUtf8Bytes() const
		{
			return MakeArrayView(Utf8Bytes);
		}

	private:
		FString LogicalPath;
		TArray<uint8> Utf8Bytes;
	};

	struct FFrontendSessionRun
	{
		TSharedRef<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot =
			MakeShared<AS_NAMESPACE_QUALIFIER asCSourceSnapshot, ESPMode::ThreadSafe>();
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager{Snapshot};
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics{SourceManager};
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCCompilationSession> Session;

		explicit FFrontendSessionRun(
			const TArray<TPair<FString, FString>>& Sources,
			TMap<FString, bool> Flags,
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy UnicodePolicy)
			: FFrontendSessionRun(Sources, Flags, nullptr, UnicodePolicy)
		{
		}

		explicit FFrontendSessionRun(
			const TArray<TPair<FString, FString>>& Sources,
			TMap<FString, bool> Flags = {},
			TSharedPtr<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> TypeContext = nullptr,
			AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy UnicodePolicy = AS_NAMESPACE_QUALIFIER asEUnicodeIdentifierPolicy::AsciiOnly)
		{
			TArray<AS_NAMESPACE_QUALIFIER asCSourceFileID> Files;
			TArray<TArray<uint8>> Encoded;
			for (const TPair<FString, FString>& Source : Sources)
			{
				const auto Input = AngelscriptNativeEngineTest::FSourceInput::FromText(Source.Key, Source.Value);
				TArray<uint8> Bytes;
				Bytes.Append(Input.GetUtf8Bytes().GetData(), Input.GetUtf8Bytes().Num());
				Encoded.Add(MoveTemp(Bytes));
				Files.Add(Snapshot->AddFile(Source.Key, Encoded.Last()));
			}
			Snapshot->Freeze();
			Session = MakeUnique<AS_NAMESPACE_QUALIFIER asCCompilationSession>(
				Snapshot,
				Diagnostics,
				TEXT("game"),
				TypeContext.IsValid()
					? TypeContext.ToSharedRef()
					: MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>());
			for (int32 Index = 0; Index < Sources.Num(); ++Index)
			{
				// The compilation session must retain everything needed after these
				// caller-owned lexer, identifier and preprocessor objects are destroyed.
				AS_NAMESPACE_QUALIFIER asCIdentifierTable Identifiers;
				AS_NAMESPACE_QUALIFIER asCTokenizer Tokenizer(
					SourceManager,
					SourceManager.MakeRange(Files[Index], 0, Encoded[Index].Num()),
					AS_NAMESPACE_QUALIFIER asSLexOptions(
						UnicodePolicy,
						AS_NAMESPACE_QUALIFIER asETriviaMode::RetainTrivia),
					Identifiers,
					Diagnostics);
				TArray<AS_NAMESPACE_QUALIFIER asCToken> Tokens;
				for (;;)
				{
					AS_NAMESPACE_QUALIFIER asCToken Token;
					if (!Tokenizer.Lex(Token)) break;
					Tokens.Add(Token);
					if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile) break;
				}
				Tokenizer.FlushDiagnostics();
				AS_NAMESPACE_QUALIFIER asCPreprocessorConfiguration Configuration(Flags);
				AS_NAMESPACE_QUALIFIER asCPreprocessor Preprocessor(SourceManager, Configuration, Diagnostics);
				const AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult Result = Preprocessor.Process(Tokens);
				Session->CollectSource(Preprocessor, Result, Sources[Index].Key);
			}
		}

		bool ResolveDeclarations()
		{
			return Session->CloseDeclarationBarrier() && Session->ResolveDeclarations(1);
		}

		AS_NAMESPACE_QUALIFIER asCFunctionDecl* Function(const TCHAR* Name) const
		{
			const TConstArrayView<AS_NAMESPACE_QUALIFIER asCDecl*> Found = Session->Lookup(Name);
			return Found.IsEmpty() ? nullptr : AS_NAMESPACE_QUALIFIER dyn_cast<AS_NAMESPACE_QUALIFIER asCFunctionDecl>(Found[0]);
		}

		const AS_NAMESPACE_QUALIFIER asCBodyFragment* BodyFragment(const TCHAR* Name) const
		{
			const AS_NAMESPACE_QUALIFIER asCFunctionDecl* Target = Function(Name);
			for (const AS_NAMESPACE_QUALIFIER asCBodyFragment& Fragment : Session->GetBodyFragments())
			{
				if (Fragment.GetWorkItem().Function == Target) return &Fragment;
			}
			return nullptr;
		}
	};

	enum class EDiagnosticSeverity : uint8
	{
		Note,
		Warning,
		Error,
	};

	struct FDiagnosticObservation
	{
		EDiagnosticSeverity Severity = EDiagnosticSeverity::Note;
		int32 ByteOffset = 0;
		FString Message;
	};

	class FDiagnosticCapture
	{
	public:
		void Add(EDiagnosticSeverity InSeverity, int32 InByteOffset, FStringView InMessage)
		{
			FDiagnosticObservation& Observation = Observations.AddDefaulted_GetRef();
			Observation.Severity = InSeverity;
			Observation.ByteOffset = InByteOffset;
			Observation.Message = FString(InMessage);
		}

		TConstArrayView<FDiagnosticObservation> GetObservations() const
		{
			return MakeArrayView(Observations);
		}

		void Reset()
		{
			Observations.Reset();
		}

	private:
		TArray<FDiagnosticObservation> Observations;
	};
}
