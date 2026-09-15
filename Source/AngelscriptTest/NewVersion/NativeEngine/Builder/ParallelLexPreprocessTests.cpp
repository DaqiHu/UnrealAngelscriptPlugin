#include "CQTest.h"
#include "../NativeEngineTestSupport.h"

#include "as_builder.h"
#include "frontend/Lexer/as_preprocessor.h"
#include "frontend/Lexer/as_token.h"
#include "frontend/Lexer/as_tokenizer.h"

#include "Containers/Set.h"
#include "HAL/PlatformTLS.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ParallelLexPreprocess,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static AS_NAMESPACE_QUALIFIER asSBuilderOptions MakeOptions(int32 Workers, int32 Batch)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("builder.parallel-lex");
		Options.WorkerCount = Workers;
		Options.LexBatchSize = Batch;
		return Options;
	}

	static FString RepeatOther(int32 Count)
	{
		FString Text;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Text += TEXT("int Other;\n");
		}
		return Text;
	}

	static TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> MakeABC(bool bReversed)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		const auto A = AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int Shared;"));
		const auto B = AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int Shared;"));
		const auto C = AngelscriptNativeEngineTest::MakeSharedSource(TEXT("C.as"), RepeatOther(100));
		if (bReversed)
		{
			Sources.Add(C);
			Sources.Add(B);
			Sources.Add(A);
		}
		else
		{
			Sources.Add(A);
			Sources.Add(B);
			Sources.Add(C);
		}
		return Sources;
	}

	static bool SpellingEquals(const AS_NAMESPACE_QUALIFIER asCIdentifierInfo* Identifier, const ANSICHAR* Text)
	{
		if (!Identifier)
		{
			return false;
		}
		const TConstArrayView<uint8> Bytes = Identifier->GetSpelling();
		const int32 Length = FCStringAnsi::Strlen(Text);
		return Bytes.Num() == Length
			&& FMemory::Memcmp(Bytes.GetData(), Text, Length) == 0;
	}

	static const AS_NAMESPACE_QUALIFIER asCIdentifierInfo* FindIdentifier(
		TConstArrayView<AS_NAMESPACE_QUALIFIER asCToken> Tokens,
		const ANSICHAR* Spelling)
	{
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Tokens)
		{
			if (Token.Kind == AS_NAMESPACE_QUALIFIER asETokenKind::Identifier
				&& SpellingEquals(Token.Identifier, Spelling))
			{
				return Token.Identifier;
			}
		}
		return nullptr;
	}

	static FString TokenProjection(
		const AS_NAMESPACE_QUALIFIER asCBuilder& Builder,
		TConstArrayView<AS_NAMESPACE_QUALIFIER asCToken> Tokens)
	{
		FString Text;
		const AS_NAMESPACE_QUALIFIER asCSourceManager* Manager = Builder.GetSourceManager();
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Tokens)
		{
			TConstArrayView<uint8> Bytes;
			FString Spelling;
			if (Manager && Manager->TryGetBytes(Token.Range, Bytes) && !Bytes.IsEmpty())
			{
				FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
				Spelling = FString(Converted.Length(), Converted.Get());
			}
			else if (Token.Identifier)
			{
				const TConstArrayView<uint8> IdentBytes = Token.Identifier->GetSpelling();
				FUTF8ToTCHAR Converted(
					reinterpret_cast<const ANSICHAR*>(IdentBytes.GetData()),
					IdentBytes.Num());
				Spelling = FString(Converted.Length(), Converted.Get());
			}
			Text += FString::Printf(
				TEXT("%u|%u:%u|%s\n"),
				uint16(Token.Kind),
				Token.Range.Begin.ByteOffset,
				Token.Range.End.ByteOffset,
				*Spelling);
		}
		return Text;
	}

	static int32 CountUniqueIdentifiers(const AS_NAMESPACE_QUALIFIER asCBuilder& Builder)
	{
		TSet<const AS_NAMESPACE_QUALIFIER asCIdentifierInfo*> Unique;
		for (const auto& Input : Builder.GetLexedSources())
		{
			for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Input.RawTokens)
			{
				if (Token.Identifier)
				{
					Unique.Add(Token.Identifier);
				}
			}
		}
		for (const auto& Processed : Builder.GetPreprocessedSources())
		{
			if (!Processed.IdentifierOwner.IsValid())
			{
				continue;
			}
			for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Processed.ActiveTokens)
			{
				if (Token.Identifier)
				{
					Unique.Add(Token.Identifier);
				}
			}
		}
		return Unique.Num();
	}

	static FString PreprocessKey(
		const AS_NAMESPACE_QUALIFIER asCBuilder& Builder,
		const AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult& Processed)
	{
		const AS_NAMESPACE_QUALIFIER asCSourceManager* Manager = Builder.GetSourceManager();
		if (!Manager)
		{
			return {};
		}
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Processed.RawTokens)
		{
			FStringView Key;
			if (Manager->TryGetLogicalSourceKey(Token.Range.Begin.FileID, Key))
			{
				return FString(Key);
			}
		}
		for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Processed.ActiveTokens)
		{
			FStringView Key;
			if (Manager->TryGetLogicalSourceKey(Token.Range.Begin.FileID, Key))
			{
				return FString(Key);
			}
		}
		return {};
	}

	static bool HasDiagnostic(
		const AS_NAMESPACE_QUALIFIER asCBuilder& Builder,
		asUINT DiagnosticID)
	{
		for (uint8 Stage = 0; Stage < uint8(AS_NAMESPACE_QUALIFIER asEBuilderStage::Failed); ++Stage)
		{
			const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage(Stage));
			if (!Result)
			{
				continue;
			}
			for (const auto& Record : Result->Diagnostics)
			{
				if (Record.DiagnosticID == DiagnosticID)
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool EndsWithEof(TConstArrayView<AS_NAMESPACE_QUALIFIER asCToken> Tokens)
	{
		return Tokens.Num() > 0
			&& Tokens.Last().Kind == AS_NAMESPACE_QUALIFIER asETokenKind::EndOfFile;
	}

	void AssertJoinedABC(const AS_NAMESPACE_QUALIFIER asCBuilder& Builder, int32 Workers)
	{
		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSSemanticSourceInput> Inputs = Builder.GetLexedSources();
		ASSERT_THAT(AreEqual(3, Inputs.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("A.as")), Inputs[0].LogicalSourceKey));
		ASSERT_THAT(AreEqual(FString(TEXT("B.as")), Inputs[1].LogicalSourceKey));
		ASSERT_THAT(AreEqual(FString(TEXT("C.as")), Inputs[2].LogicalSourceKey));
		ASSERT_THAT(IsTrue(EndsWithEof(Inputs[0].RawTokens)));
		ASSERT_THAT(IsTrue(EndsWithEof(Inputs[1].RawTokens)));
		ASSERT_THAT(IsTrue(EndsWithEof(Inputs[2].RawTokens)));

		const AS_NAMESPACE_QUALIFIER asCIdentifierInfo* SharedA = FindIdentifier(Inputs[0].RawTokens, "Shared");
		const AS_NAMESPACE_QUALIFIER asCIdentifierInfo* SharedB = FindIdentifier(Inputs[1].RawTokens, "Shared");
		ASSERT_THAT(IsNotNull(SharedA));
		ASSERT_THAT(IsNotNull(SharedB));
		ASSERT_THAT(IsTrue(SharedA == SharedB));

		ASSERT_THAT(IsTrue(Builder.DebugGetPeakWorkers() <= Workers));
		if (Workers <= 1)
		{
			const uint32 Caller = Builder.DebugGetLexCallerThreadId();
			ASSERT_THAT(IsTrue(Caller != 0));
			for (const uint32 ThreadId : Builder.DebugGetWorkThreadIds())
			{
				ASSERT_THAT(AreEqual(Caller, ThreadId));
			}
		}
	}

	void AssertHardFailureDrains(bool bAcquisition)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int A;")));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("C.as"), TEXT("int C;")));
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, MakeOptions(4, 1));
		if (bAcquisition)
		{
			Builder.DebugForceAcquisitionFailure(TEXT("A.as"));
		}
		else
		{
			Builder.DebugForceLexFalse(TEXT("A.as"));
		}
		ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));

		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSSemanticSourceInput> Inputs = Builder.GetLexedSources();
		ASSERT_THAT(AreEqual(3, Inputs.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("A.as")), Inputs[0].LogicalSourceKey));
		ASSERT_THAT(AreEqual(FString(TEXT("B.as")), Inputs[1].LogicalSourceKey));
		ASSERT_THAT(AreEqual(FString(TEXT("C.as")), Inputs[2].LogicalSourceKey));
		ASSERT_THAT(IsTrue(EndsWithEof(Inputs[1].RawTokens)));
		ASSERT_THAT(IsTrue(EndsWithEof(Inputs[2].RawTokens)));

		TArray<FString> Processed = Builder.DebugGetProcessedLogicalKeys();
		Processed.Sort();
		ASSERT_THAT(AreEqual(2, Processed.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("B.as")), Processed[0]));
		ASSERT_THAT(AreEqual(FString(TEXT("C.as")), Processed[1]));

		bool bSawB = false;
		bool bSawC = false;
		for (const auto& Result : Builder.GetPreprocessedSources())
		{
			const FString Key = PreprocessKey(Builder, Result);
			bSawB = bSawB || Key == TEXT("B.as");
			bSawC = bSawC || Key == TEXT("C.as");
			ASSERT_THAT(IsFalse(Key == TEXT("A.as")));
		}
		ASSERT_THAT(IsTrue(bSawB));
		ASSERT_THAT(IsTrue(bSawC));
	}

public:
	TEST_METHOD(QueueAndSpellingIdentity)
	{
		const int32 Settings[][2] = {{1, 1}, {1, 4}, {4, 1}, {4, 4}};
		FString SingleWorkerProjection;
		int32 SingleWorkerUnique = 0;
		for (const int32* Setting : Settings)
		{
			const int32 Workers = Setting[0];
			const int32 Batch = Setting[1];
			for (const bool bReversed : {false, true})
			{
				AS_NAMESPACE_QUALIFIER asCBuilder Builder(MakeABC(bReversed), MakeOptions(Workers, Batch));
				ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
				AssertJoinedABC(Builder, Workers);
				const FString Projection =
					TokenProjection(Builder, Builder.GetLexedSources()[0].RawTokens)
					+ TokenProjection(Builder, Builder.GetLexedSources()[1].RawTokens)
					+ TokenProjection(Builder, Builder.GetLexedSources()[2].RawTokens);
				const int32 Unique = CountUniqueIdentifiers(Builder);
				if (Workers == 1 && Batch == 1 && !bReversed)
				{
					SingleWorkerProjection = Projection;
					SingleWorkerUnique = Unique;
				}
				else
				{
					ASSERT_THAT(AreEqual(SingleWorkerProjection, Projection));
					ASSERT_THAT(AreEqual(SingleWorkerUnique, Unique));
				}
			}
		}
	}

	TEST_METHOD(LocalGateAndCleanProductIdentity)
	{
		const uint8 Unterminated[] = {0x22, 0x61, 0x62, 0x63};
		const int32 Settings[][2] = {{1, 1}, {1, 4}, {4, 1}, {4, 4}};
		for (const int32* Setting : Settings)
		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSourceBytes(
				TEXT("A.as"),
				MakeArrayView(Unterminated)));
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, MakeOptions(Setting[0], Setting[1]));
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
			ASSERT_THAT(IsTrue(HasDiagnostic(
				Builder,
				static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::UnterminatedString))));

			const TConstArrayView<AS_NAMESPACE_QUALIFIER asSSemanticSourceInput> Inputs = Builder.GetLexedSources();
			ASSERT_THAT(AreEqual(2, Inputs.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("A.as")), Inputs[0].LogicalSourceKey));
			ASSERT_THAT(AreEqual(FString(TEXT("B.as")), Inputs[1].LogicalSourceKey));

			bool bSawA = false;
			bool bSawB = false;
			for (const auto& Processed : Builder.GetPreprocessedSources())
			{
				const FString Key = PreprocessKey(Builder, Processed);
				ASSERT_THAT(IsFalse(Key == TEXT("A.as")));
				if (Key == TEXT("B.as"))
				{
					bSawB = true;
					ASSERT_THAT(IsTrue(Processed.IsValid()));
				}
				else
				{
					bSawA = true;
				}
			}
			ASSERT_THAT(IsFalse(bSawA));
			ASSERT_THAT(IsTrue(bSawB));
			ASSERT_THAT(AreEqual(1, Builder.GetPreprocessedSources().Num()));
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsCollected)));
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsBuilt)));
		}
	}

	TEST_METHOD(HardFailureDrainsWork)
	{
		AssertHardFailureDrains(false);
		AssertHardFailureDrains(true);
	}

	TEST_METHOD(PPFailureAndExactlyOnceAdvancement)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> FailedSources;
		FailedSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(
			TEXT("A.as"),
			TEXT("#if defined(\n#endif\n")));
		FailedSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));
		AS_NAMESPACE_QUALIFIER asCBuilder Failed(FailedSources, MakeOptions(2, 1));
		ASSERT_THAT(IsFalse(Failed.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		ASSERT_THAT(IsTrue(HasDiagnostic(
			Failed,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEPreprocessorDiagnosticID::MalformedCondition))));
		bool bSawValidB = false;
		for (const auto& Processed : Failed.GetPreprocessedSources())
		{
			const FString Key = PreprocessKey(Failed, Processed);
			if (Key == TEXT("B.as"))
			{
				bSawValidB = Processed.IsValid();
			}
		}
		ASSERT_THAT(IsTrue(bSawValidB));

		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> CleanSources;
		CleanSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("int A;")));
		CleanSources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("B.as"), TEXT("int B;")));

		AS_NAMESPACE_QUALIFIER asCBuilder Through(CleanSources, MakeOptions(2, 2));
		ASSERT_THAT(IsTrue(Through.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
		ASSERT_THAT(AreEqual(2, Through.DebugGetProcessInvocationCount()));

		AS_NAMESPACE_QUALIFIER asCBuilder Staged(CleanSources, MakeOptions(2, 2));
		ASSERT_THAT(IsTrue(Staged.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		const int32 AfterLexed = Staged.DebugGetProcessInvocationCount();
		ASSERT_THAT(AreEqual(2, AfterLexed));
		const FString AfterLexedA = TokenProjection(Staged, Staged.GetPreprocessedSources()[0].ActiveTokens);
		const FString AfterLexedB = TokenProjection(Staged, Staged.GetPreprocessedSources()[1].ActiveTokens);
		ASSERT_THAT(IsTrue(Staged.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
		ASSERT_THAT(AreEqual(AfterLexed, Staged.DebugGetProcessInvocationCount()));
		ASSERT_THAT(AreEqual(AfterLexedA, TokenProjection(Through, Through.GetPreprocessedSources()[0].ActiveTokens)));
		ASSERT_THAT(AreEqual(AfterLexedB, TokenProjection(Through, Through.GetPreprocessedSources()[1].ActiveTokens)));
		ASSERT_THAT(AreEqual(AfterLexedA, TokenProjection(Staged, Staged.GetPreprocessedSources()[0].ActiveTokens)));
		ASSERT_THAT(AreEqual(AfterLexedB, TokenProjection(Staged, Staged.GetPreprocessedSources()[1].ActiveTokens)));

		const TConstArrayView<AS_NAMESPACE_QUALIFIER asSDirectiveProcessResult> Frozen =
			Staged.GetPreprocessedSources();
		ASSERT_THAT(AreEqual(2, Frozen.Num()));
		ASSERT_THAT(IsFalse(Staged.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::Preprocessed)));
		ASSERT_THAT(AreEqual(2, Staged.GetPreprocessedSources().Num()));
		ASSERT_THAT(AreEqual(
			TokenProjection(Staged, Frozen[0].ActiveTokens),
			TokenProjection(Staged, Staged.GetPreprocessedSources()[0].ActiveTokens)));
		ASSERT_THAT(IsFalse(Staged.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		ASSERT_THAT(AreEqual(AfterLexed, Staged.DebugGetProcessInvocationCount()));
	}

	TEST_METHOD(BoundsAndRetainedOwners)
	{
		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Empty;
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Empty, MakeOptions(4, 4));
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
			ASSERT_THAT(AreEqual(0, Builder.DebugGetPeakWorkers()));
			ASSERT_THAT(AreEqual(0, Builder.DebugGetProcessInvocationCount()));
			ASSERT_THAT(AreEqual(0, Builder.DebugGetWorkThreadIds().Num()));
		}

		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources = MakeABC(false);
		AS_NAMESPACE_QUALIFIER asCBuilder Normalized(Sources, MakeOptions(0, 0));
		ASSERT_THAT(IsTrue(Normalized.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		AssertJoinedABC(Normalized, 1);

		AS_NAMESPACE_QUALIFIER asCBuilder Single(Sources, MakeOptions(1, 1));
		ASSERT_THAT(IsTrue(Single.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		const int32 SingleUnique = CountUniqueIdentifiers(Single);

		AS_NAMESPACE_QUALIFIER asCBuilder Parallel(Sources, MakeOptions(4, 4));
		ASSERT_THAT(IsTrue(Parallel.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed)));
		ASSERT_THAT(AreEqual(SingleUnique, CountUniqueIdentifiers(Parallel)));

		const AS_NAMESPACE_QUALIFIER asCIdentifierInfo* Shared =
			FindIdentifier(Parallel.GetLexedSources()[0].RawTokens, "Shared");
		ASSERT_THAT(IsNotNull(Shared));
		const TConstArrayView<uint8> Spelling = Shared->GetSpelling();
		ASSERT_THAT(AreEqual(6, Spelling.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>('S'), Spelling[0]));
		ASSERT_THAT(IsTrue(Parallel.GetPreprocessedSources().Num() >= 1));
		ASSERT_THAT(IsTrue(Parallel.GetPreprocessedSources()[0].IdentifierOwner.IsValid()));
	}
};

#endif
