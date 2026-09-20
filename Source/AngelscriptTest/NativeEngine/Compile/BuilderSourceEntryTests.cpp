#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "as_builder.h"
#include "frontend/AST/as_decl.h"
#include "frontend/Lexer/as_tokenizer.h"
#include "Math/NumericLimits.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	BuilderSourceEntry,
	"Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static AS_NAMESPACE_QUALIFIER asSBuilderOptions MakeOptions(const TCHAR* Scope = TEXT("builder.source-entry"), int32 Workers = 1)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = Scope;
		Options.WorkerCount = Workers;
		return Options;
	}

	static bool SliceEquals(TConstArrayView<uint8> Bytes, const ANSICHAR* Expected, int32 ExpectedLen)
	{
		return Bytes.Num() == ExpectedLen
			&& FMemory::Memcmp(Bytes.GetData(), Expected, ExpectedLen) == 0;
	}

	static bool AdmissionFailed(const AS_NAMESPACE_QUALIFIER asCBuilder& Builder)
	{
		const auto* Result = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady);
		return Result != nullptr && !Result->Succeeded && !Result->Error.IsEmpty();
	}

public:
	TEST_METHOD(SharedBodyThroughFrontend)
	{
		FUtf8String Body = AngelscriptNativeEngineTest::MakeUtf8FromText(TEXT("class Unit {}"));
		const UTF8CHAR* const Allocated = *Body;
		const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> Source =
			MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(FString(TEXT("A.as")), MoveTemp(Body));
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(Source);
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, MakeOptions());
		Sources.Reset();

		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		const AS_NAMESPACE_QUALIFIER asCSourceManager* Manager = Builder.GetSourceManager();
		ASSERT_THAT(IsNotNull(Manager));

		const AS_NAMESPACE_QUALIFIER asCSourceRange UnitRange = Manager->MakeRange(
			AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 6, 10);
		TConstArrayView<uint8> Slice;
		ASSERT_THAT(IsTrue(Manager->TryGetBytes(UnitRange, Slice)));
		ASSERT_THAT(IsTrue(SliceEquals(Slice, "Unit", 4)));
		ASSERT_THAT(IsTrue(Slice.GetData() == reinterpret_cast<const uint8*>(Allocated) + 6));

		bool bFoundToken = false;
		for (const auto& Input : Builder.GetLexedSources())
		{
			for (const AS_NAMESPACE_QUALIFIER asCToken& Token : Input.RawTokens)
			{
				TConstArrayView<uint8> TokenBytes;
				if (!Manager->TryGetBytes(Token.Range, TokenBytes) || !SliceEquals(TokenBytes, "Unit", 4))
				{
					continue;
				}
				ASSERT_THAT(IsTrue(TokenBytes.GetData() == reinterpret_cast<const uint8*>(Allocated) + 6));
				bFoundToken = true;
			}
		}
		ASSERT_THAT(IsTrue(bFoundToken));
	}

	TEST_METHOD(SeparateInputIdentitySameSource)
	{
		const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> Source =
			AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("class Unit {}"));
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> FirstSources;
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> SecondSources;
		FirstSources.Add(Source);
		SecondSources.Add(Source);
		AS_NAMESPACE_QUALIFIER asCBuilder First(FirstSources, MakeOptions(TEXT("builder.first")));
		AS_NAMESPACE_QUALIFIER asCBuilder Second(SecondSources, MakeOptions(TEXT("builder.second")));
		FirstSources.Reset();
		SecondSources.Reset();

		ASSERT_THAT(IsTrue(First.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		ASSERT_THAT(IsTrue(Second.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		const AS_NAMESPACE_QUALIFIER asCSourceManager* FirstManager = First.GetSourceManager();
		const AS_NAMESPACE_QUALIFIER asCSourceManager* SecondManager = Second.GetSourceManager();
		ASSERT_THAT(IsNotNull(FirstManager));
		ASSERT_THAT(IsNotNull(SecondManager));
		ASSERT_THAT(IsTrue(FirstManager->GetSnapshotID() != SecondManager->GetSnapshotID()));

		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileID(1);
		const AS_NAMESPACE_QUALIFIER asCSourceRange FirstRange = FirstManager->MakeRange(FileID, 6, 10);
		const AS_NAMESPACE_QUALIFIER asCSourceRange SecondRange = SecondManager->MakeRange(FileID, 6, 10);
		TConstArrayView<uint8> FirstSlice;
		TConstArrayView<uint8> SecondSlice;
		ASSERT_THAT(IsTrue(FirstManager->TryGetBytes(FirstRange, FirstSlice)));
		ASSERT_THAT(IsTrue(SecondManager->TryGetBytes(SecondRange, SecondSlice)));
		ASSERT_THAT(IsTrue(SliceEquals(FirstSlice, "Unit", 4)));
		ASSERT_THAT(IsTrue(SliceEquals(SecondSlice, "Unit", 4)));
		ASSERT_THAT(IsTrue(FirstSlice.GetData() == SecondSlice.GetData()));
		ASSERT_THAT(AreEqual(FileID.Value, FirstRange.Begin.FileID.Value));
		ASSERT_THAT(AreEqual(FileID.Value, SecondRange.Begin.FileID.Value));

		const AS_NAMESPACE_QUALIFIER asCSourceRange Mixed(FirstRange.Begin, SecondRange.End);
		TConstArrayView<uint8> MixedSlice;
		ASSERT_THAT(IsFalse(FirstManager->TryGetBytes(Mixed, MixedSlice)));
		ASSERT_THAT(IsFalse(SecondManager->TryGetBytes(Mixed, MixedSlice)));
	}

	TEST_METHOD(RawCoordinatesAndDiagnostics)
	{
		const uint8 Raw[] = {0xEF, 0xBB, 0xBF, 0x61, 0x0D, 0x0A, 0xC3, 0xA9, 0x00, 0xFF};
		const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> Source =
			AngelscriptNativeEngineTest::MakeSharedSourceBytes(TEXT("A.as"), MakeArrayView(Raw));
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
		Sources.Add(Source);
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, MakeOptions());
		Sources.Reset();

		Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed);
		const AS_NAMESPACE_QUALIFIER asCSourceManager* Manager = Builder.GetSourceManager();
		ASSERT_THAT(IsNotNull(Manager));

		TConstArrayView<uint8> Body;
		ASSERT_THAT(IsTrue(Manager->TryGetBytes(
			Manager->MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 0, 10),
			Body)));
		ASSERT_THAT(AreEqual(10, Body.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Body.GetData(), Raw, 10) == 0));

		TConstArrayView<uint8> Accent;
		ASSERT_THAT(IsTrue(Manager->TryGetBytes(
			Manager->MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 6, 8),
			Accent)));
		const uint8 ExpectedAccent[] = {0xC3, 0xA9};
		ASSERT_THAT(AreEqual(2, Accent.Num()));
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Accent.GetData(), ExpectedAccent, 2) == 0));

		TConstArrayView<uint8> Nul;
		ASSERT_THAT(IsTrue(Manager->TryGetBytes(
			Manager->MakeRange(AS_NAMESPACE_QUALIFIER asCSourceFileID(1), 8, 9),
			Nul)));
		ASSERT_THAT(AreEqual(1, Nul.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Nul[0]));

		const auto* Lexed = Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::Lexed);
		ASSERT_THAT(IsNotNull(Lexed));
		bool bSawNul = false;
		bool bSawInvalidUtf8 = false;
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticRecord& Record : Lexed->Diagnostics)
		{
			if (Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::EmbeddedNul)
				&& Record.PrimaryRange.Begin.ByteOffset == 8)
			{
				bSawNul = true;
			}
			if (Record.DiagnosticID == static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asELexDiagnosticID::InvalidUtf8)
				&& Record.PrimaryRange.Begin.ByteOffset == 9)
			{
				bSawInvalidUtf8 = true;
			}
		}
		ASSERT_THAT(IsTrue(bSawNul));
		ASSERT_THAT(IsTrue(bSawInvalidUtf8));
	}

	TEST_METHOD(InputAdmission)
	{
		const auto Options = MakeOptions();

		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Empty;
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Empty, Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(AdmissionFailed(Builder)));
		}

		{
			FAngelscriptSource Unprepared;
			Unprepared.AbsoluteFilename = TEXT("C:/DoesNotExist/A.as");
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(MakeShared<FAngelscriptSource, ESPMode::ThreadSafe>(MoveTemp(Unprepared)));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(AdmissionFailed(Builder)));
		}

		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT(""), TEXT("class Unit {}")));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(AdmissionFailed(Builder)));
		}

		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("class Unit {}")));
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("class Other {}")));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(AdmissionFailed(Builder)));
		}

		{
			TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources;
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("A.as"), TEXT("")));
			Sources.Add(AngelscriptNativeEngineTest::MakeSharedSource(TEXT("a.as"), TEXT("")));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(Sources, Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady)));
			ASSERT_THAT(AreEqual(
				AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady,
				Builder.GetStage()));
		}
	}

	TEST_METHOD(OptionsAndLengthAdmission)
	{
		{
			AS_NAMESPACE_QUALIFIER asSBuilderOptions Options = MakeOptions(TEXT(""));
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(
				AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
				Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough()));
			ASSERT_THAT(IsTrue(AdmissionFailed(Builder)));
		}

		{
			AS_NAMESPACE_QUALIFIER asSBuilderOptions Options = MakeOptions();
			Options.WorkerCount = 0;
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(
				AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
				Options);
			ASSERT_THAT(IsTrue(Builder.RunThrough()));
			ASSERT_THAT(IsFalse(AdmissionFailed(Builder)));
		}

		asUINT Converted = 42;
		ASSERT_THAT(IsFalse(AS_NAMESPACE_QUALIFIER asTryConvertSourceByteLength(
			static_cast<uint64>(TNumericLimits<asUINT>::Max()) + 1,
			Converted)));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(0), Converted));
	}

	TEST_METHOD(StageProgression)
	{
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
			MakeOptions());
		const uint64 Identity = Builder.GetSourceManager()->GetSnapshotID();

		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved, Builder.GetStage()));
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved, Builder.GetStage()));
		ASSERT_THAT(IsTrue(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed, Builder.GetStage()));
		ASSERT_THAT(IsTrue(Builder.RunThrough()));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted, Builder.GetStage()));
		ASSERT_THAT(AreEqual(Identity, Builder.GetSourceManager()->GetSnapshotID()));

		AS_NAMESPACE_QUALIFIER asCBuilder Skip(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
			MakeOptions(TEXT("builder.skip")));
		ASSERT_THAT(IsTrue(Skip.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady)));
		ASSERT_THAT(IsFalse(Skip.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady, Skip.GetStage()));
		ASSERT_THAT(IsFalse(Skip.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady)));
		ASSERT_THAT(IsNull(Skip.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
	}

	TEST_METHOD(RetainedAstAndLazyLines)
	{
		TArray<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources =
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}\r\n"));
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCBuilder> Builder =
			MakeUnique<AS_NAMESPACE_QUALIFIER asCBuilder>(Sources, MakeOptions());
		Sources.Reset();
		ASSERT_THAT(IsTrue(Builder->RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));

		const AS_NAMESPACE_QUALIFIER asCCompilationSession* Session = Builder->GetCompilationSession();
		ASSERT_THAT(IsNotNull(Session));
		const AS_NAMESPACE_QUALIFIER asCTranslationUnitDecl* Root = Session->GetTranslationUnit();
		ASSERT_THAT(IsNotNull(Root));
		AS_NAMESPACE_QUALIFIER asCSourceManager Lease = Session->GetASTContext().GetSources();
		const AS_NAMESPACE_QUALIFIER asCSourceRange Range = Root->GetSourceRange();
		AS_NAMESPACE_QUALIFIER asSStableSourceAnchor Anchor;
		ASSERT_THAT(IsTrue(Lease.TryCreateStableAnchor(Range, Anchor)));
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileID = Range.Begin.FileID;
		ASSERT_THAT(IsFalse(Lease.HasMaterializedLineMap(FileID)));

		Builder.Reset();

		TConstArrayView<uint8> Bytes;
		ASSERT_THAT(IsTrue(Lease.TryGetBytes(Range, Bytes)));
		AS_NAMESPACE_QUALIFIER asCSourceRange Relocated;
		ASSERT_THAT(IsTrue(Lease.TryRelocateStableAnchor(Anchor, Relocated)));
		ASSERT_THAT(AreEqual(Range.Begin.ByteOffset, Relocated.Begin.ByteOffset));
		ASSERT_THAT(AreEqual(Range.End.ByteOffset, Relocated.End.ByteOffset));

		AS_NAMESPACE_QUALIFIER asSSourceLineColumn First;
		AS_NAMESPACE_QUALIFIER asSSourceLineColumn Second;
		ASSERT_THAT(IsTrue(Lease.TryGetLineColumn(Range.Begin, First)));
		ASSERT_THAT(IsTrue(Lease.TryGetLineColumn(Range.Begin, Second)));
		ASSERT_THAT(AreEqual(First.Line, Second.Line));
		ASSERT_THAT(AreEqual(First.Column, Second.Column));
		ASSERT_THAT(IsTrue(Lease.HasMaterializedLineMap(FileID)));
	}

	TEST_METHOD(InternalProvenanceStillQueries)
	{
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
			MakeOptions());
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DeclarationsResolved)));
		AS_NAMESPACE_QUALIFIER asCSourceManager Sources = *Builder.GetSourceManager();
		const AS_NAMESPACE_QUALIFIER asCSourceFileID FileID(1);
		const AS_NAMESPACE_QUALIFIER asCSourceRange Direct = Sources.MakeRange(FileID, 0, 13);
		const AS_NAMESPACE_QUALIFIER asCSourceRange Unit = Sources.MakeRange(FileID, 6, 10);

		AS_NAMESPACE_QUALIFIER asSSourceOriginRecord DirectOrigin;
		DirectOrigin.Kind = AS_NAMESPACE_QUALIFIER asESourceOriginKind::Direct;
		DirectOrigin.ProducedRange = Direct;
		DirectOrigin.Producer = TEXT("input");
		const AS_NAMESPACE_QUALIFIER asCSourceOriginID DirectID = Sources.AddOrigin(DirectOrigin);
		ASSERT_THAT(IsTrue(DirectID.IsValid()));

		AS_NAMESPACE_QUALIFIER asSSourceOriginRecord Transformed;
		Transformed.Kind = AS_NAMESPACE_QUALIFIER asESourceOriginKind::Transformed;
		Transformed.ProducedRange = Unit;
		Transformed.ParentRange = Direct;
		Transformed.ParentOrigin = DirectID;
		Transformed.Producer = TEXT("test-transform");
		const AS_NAMESPACE_QUALIFIER asCSourceOriginID TransformedID = Sources.AddOrigin(Transformed);
		ASSERT_THAT(IsTrue(TransformedID.IsValid()));

		const AS_NAMESPACE_QUALIFIER asCTranslationUnitDecl* Root =
			Builder.GetCompilationSession()->GetTranslationUnit();
		ASSERT_THAT(IsNotNull(Root));
		TArray<AS_NAMESPACE_QUALIFIER asSSourceOriginRecord> Found;
		ASSERT_THAT(IsTrue(Sources.FindOrigins(Root->GetSourceRange(), Found)));
		ASSERT_THAT(IsTrue(Found.Num() >= 1));

		TArray<AS_NAMESPACE_QUALIFIER asSSourceOriginRecord> Chain;
		ASSERT_THAT(IsTrue(Sources.TryResolveOriginChain(TransformedID, Chain)));
		ASSERT_THAT(AreEqual(2, Chain.Num()));
		TConstArrayView<uint8> Terminal;
		ASSERT_THAT(IsTrue(Sources.TryGetBytes(Chain.Last().ProducedRange, Terminal)));
		ASSERT_THAT(IsTrue(SliceEquals(Terminal, "class Unit {}", 13)));

		AS_NAMESPACE_QUALIFIER asCBuilder Foreign(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("A.as"), TEXT("class Unit {}")),
			MakeOptions(TEXT("builder.foreign")));
		ASSERT_THAT(IsTrue(Foreign.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady)));
		TArray<AS_NAMESPACE_QUALIFIER asSSourceOriginRecord> ForeignChain;
		ASSERT_THAT(IsFalse(Foreign.GetSourceManager()->TryResolveOriginChain(TransformedID, ForeignChain)));
	}
};

#endif
