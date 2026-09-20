#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Containers/Utf8String.h"
#include "frontend/AST/as_ast_cast.h"
#include "frontend/Compile/as_compilation_session.h"
#include "frontend/Basic/as_identifier_table.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "unreal/AngelscriptSource.h"

namespace AngelscriptNativeEngineTest
{
	inline FUtf8String MakeUtf8FromBytes(TConstArrayView<uint8> Bytes)
	{
		FUtf8String Body;
		if (Bytes.Num() > 0)
		{
			Body.AppendChars(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
		}
		return Body;
	}

	inline FUtf8String MakeUtf8FromText(FStringView Text)
	{
		const FTCHARToUTF8 Converted(Text.GetData(), Text.Len());
		return MakeUtf8FromBytes(TConstArrayView<uint8>(
			reinterpret_cast<const uint8*>(Converted.Get()),
			Converted.Length()));
	}

	inline TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> MakeSharedSource(
		FStringView Path,
		FStringView Text)
	{
		return MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(
			FString(Path),
			MakeUtf8FromText(Text));
	}

	inline TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> MakeSharedSourceBytes(
		FStringView Path,
		TConstArrayView<uint8> Bytes)
	{
		return MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(
			FString(Path),
			MakeUtf8FromBytes(Bytes));
	}

	inline TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> MakeSourceList(
		FStringView Path,
		FStringView Text)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(MakeSharedSource(Path, Text));
		return Sources;
	}

	inline AS_NAMESPACE_QUALIFIER asCSourceManager MakeSourceManager(
		FStringView Path,
		FStringView Text,
		AS_NAMESPACE_QUALIFIER asCSourceFileID* OutFileID = nullptr)
	{
		AS_NAMESPACE_QUALIFIER asCSourceManager Manager(MakeSourceList(Path, Text));
		if (OutFileID)
		{
			*OutFileID = AS_NAMESPACE_QUALIFIER asCSourceFileID(Manager.IsReady() ? 1 : 0);
		}
		return Manager;
	}

	inline AS_NAMESPACE_QUALIFIER asCSourceManager MakeSourceManagerFromBytes(
		FStringView Path,
		TConstArrayView<uint8> Bytes,
		AS_NAMESPACE_QUALIFIER asCSourceFileID* OutFileID = nullptr)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(MakeSharedSourceBytes(Path, Bytes));
		AS_NAMESPACE_QUALIFIER asCSourceManager Manager(Sources);
		if (OutFileID)
		{
			*OutFileID = AS_NAMESPACE_QUALIFIER asCSourceFileID(Manager.IsReady() ? 1 : 0);
		}
		return Manager;
	}

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
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SharedSources;
		AS_NAMESPACE_QUALIFIER asCSourceManager SourceManager;
		AS_NAMESPACE_QUALIFIER asCDiagnosticsEngine Diagnostics;
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
			: SharedSources(MakeSharedSources(Sources))
			, SourceManager(SharedSources)
			, Diagnostics(SourceManager)
		{
			TArray<AS_NAMESPACE_QUALIFIER asCSourceFileID> Files;
			TArray<int32> Lengths;
			for (int32 Index = 0; Index < Sources.Num(); ++Index)
			{
				Files.Add(AS_NAMESPACE_QUALIFIER asCSourceFileID(static_cast<asUINT>(Index + 1)));
				Lengths.Add(SharedSources[Index]->GetSourceText().Len());
			}
			Session = MakeUnique<AS_NAMESPACE_QUALIFIER asCCompilationSession>(
				SourceManager,
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
					SourceManager.MakeRange(Files[Index], 0, static_cast<asUINT>(Lengths[Index])),
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

		static TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> MakeSharedSources(
			const TArray<TPair<FString, FString>>& Sources)
		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Result;
			for (const TPair<FString, FString>& Source : Sources)
			{
				Result.Add(MakeSharedSource(Source.Key, Source.Value));
			}
			return Result;
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
