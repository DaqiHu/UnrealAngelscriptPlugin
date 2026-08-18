#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptPreprocessorGeneratedSourceProvenanceTests
{
	static const FAngelscriptProcessedSourceProvenanceRange* FindGeneratedRange(
		const FAngelscriptModuleDesc::FCodeSection& Section,
		const EAngelscriptGeneratedSourceKind Kind)
	{
		return Section.SourceProvenance.FindByPredicate(
			[Kind](const FAngelscriptProcessedSourceProvenanceRange& Range)
			{
				return Range.bHasGeneratedOrigin
					&& Range.GeneratedKind == Kind;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorGeneratedSourceProvenanceTests,
	"Angelscript.TestModule.Preprocessor.SourceProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LiteralAssetAndSubsystemGeneratedRangesRetainHonestAnchors)
	{
		using namespace AngelscriptPreprocessorGeneratedSourceProvenanceTests;
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			FFixtureFile File(
				TEXT("Tests/Preprocessor/SourceProvenance/Generated.as"),
				TEXT(R"AS(
// 非 ASCII 前缀验证 TCHAR/UTF-8 offset 不会混用。
UCLASS()
class USourceProvenanceSubsystem : UEngineSubsystem
{
}

asset SourceProvenanceAsset of UObject
{
			}
			)AS"));
			FString AuthoredSource;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
				AuthoredSource,
				*File.AbsolutePath)));
			const FPreprocessSession Session = RunPreprocessSession(Engine, File);
			AssertPreprocessSucceeded(*TestRunner, Session.Result);
			AssertModuleCount(*TestRunner, Session.Result, 1);
			AssertNoDiagnostics(*TestRunner, Session.Result);

			const FAngelscriptModuleDesc* const Module = AssertModuleExists(
				*TestRunner,
				Session.Result,
				TEXT("Tests.Preprocessor.SourceProvenance.Generated"));
			ASSERT_THAT(IsNotNull(Module));
			if (Module == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(1, Module->Code.Num()));
			if (Module->Code.Num() != 1)
			{
				return;
			}

			const FAngelscriptModuleDesc::FCodeSection& Section = Module->Code[0];
			const FAngelscriptProcessedSourceProvenanceRange* const Literal =
				FindGeneratedRange(Section,
					EAngelscriptGeneratedSourceKind::LiteralAsset);
			const FAngelscriptProcessedSourceProvenanceRange* const Subsystem =
				FindGeneratedRange(Section,
					EAngelscriptGeneratedSourceKind::SubsystemAccessor);
			ASSERT_THAT(IsNotNull(Literal));
			ASSERT_THAT(IsNotNull(Subsystem));
			if (Literal == nullptr || Subsystem == nullptr)
			{
				return;
			}

			ASSERT_THAT(IsTrue(Literal->ProcessedCodeUnitLength > 0));
			ASSERT_THAT(IsTrue(Subsystem->ProcessedCodeUnitLength > 0));
			ASSERT_THAT(IsTrue(Section.Code.Mid(
				Literal->ProcessedCodeUnitOffset,
				Literal->ProcessedCodeUnitLength).Contains(
					TEXT("GetSourceProvenanceAsset"))));
			ASSERT_THAT(IsTrue(Section.Code.Mid(
				Subsystem->ProcessedCodeUnitOffset,
				Subsystem->ProcessedCodeUnitLength).Contains(TEXT("Get()"))));

			ASSERT_THAT(AreEqual(
				FString(TEXT("LiteralAsset:SourceProvenanceAsset")),
				Literal->GeneratorName));
			ASSERT_THAT(AreEqual(
				FString(TEXT("SubsystemAccessor:USourceProvenanceSubsystem")),
				Subsystem->GeneratorName));
			ASSERT_THAT(IsTrue(Literal->bHasGeneratedAuthoredAnchor));
			ASSERT_THAT(IsTrue(Subsystem->bHasGeneratedAuthoredAnchor));
			ASSERT_THAT(AreEqual(7u, Literal->GeneratedAuthoredAnchor.Row));
			ASSERT_THAT(AreEqual(3u, Subsystem->GeneratedAuthoredAnchor.Row));

			// Authored anchors are already UTF-8 byte offsets; the Chinese comment
			// makes this differ from the corresponding TCHAR code-unit offset.
			const int32 AuthoredClassOffset = AuthoredSource.Find(
				TEXT("class USourceProvenanceSubsystem"));
			ASSERT_THAT(IsTrue(AuthoredClassOffset != INDEX_NONE));
			const FString AuthoredPrefix = AuthoredSource.Left(AuthoredClassOffset);
			const FTCHARToUTF8 AuthoredPrefixUtf8(*AuthoredPrefix);
			ASSERT_THAT(AreEqual(
				static_cast<uint32>(AuthoredPrefixUtf8.Length()),
				Subsystem->GeneratedAuthoredAnchor.SourceOffset));
			ASSERT_THAT(IsTrue(
				Subsystem->GeneratedAuthoredAnchor.SourceOffset
					> static_cast<uint32>(AuthoredClassOffset)));
		}
	}

	TEST_METHOD(PostProcessCodeReplacementKeepsTrailingGeneratedRangesInBounds)
	{
		using namespace AngelscriptPreprocessorGeneratedSourceProvenanceTests;
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{
			FAngelscriptEngineScope EngineScope(Engine);
			FScopedModuleCleanEngine ModuleClean(Engine);

			int32 ReplacementCount = 0;
			const FDelegateHandle HookHandle =
				FAngelscriptPreprocessor::OnPostProcessCode.AddLambda(
					[&ReplacementCount](FAngelscriptPreprocessor& Preprocessor)
					{
						for (FAngelscriptPreprocessor::FFile& File
							: Preprocessor.Files)
						{
							ReplacementCount += File.ProcessedCode.ReplaceInline(
								TEXT("CACHE_EXTERNAL_VALUE"),
								TEXT("7"),
								ESearchCase::CaseSensitive);
						}
					});
			ON_SCOPE_EXIT
			{
				FAngelscriptPreprocessor::OnPostProcessCode.Remove(HookHandle);
			};

			FFixtureFile File(
				TEXT("Tests/Preprocessor/SourceProvenance/PostProcessHook.as"),
				TEXT(R"AS(
class FCachePayload
{
	int Count;
}

int GetCacheAnswer()
{
	return CACHE_EXTERNAL_VALUE;
}
			)AS"));
			const FPreprocessSession Session = RunPreprocessSession(Engine, File);
			AssertPreprocessSucceeded(*TestRunner, Session.Result);
			AssertModuleCount(*TestRunner, Session.Result, 1);
			AssertNoDiagnostics(*TestRunner, Session.Result);
			ASSERT_THAT(AreEqual(1, ReplacementCount));

			const FAngelscriptModuleDesc* const Module = AssertModuleExists(
				*TestRunner,
				Session.Result,
				TEXT("Tests.Preprocessor.SourceProvenance.PostProcessHook"));
			ASSERT_THAT(IsNotNull(Module));
			if (Module == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(1, Module->Code.Num()));
			if (Module->Code.Num() != 1)
			{
				return;
			}

			const FAngelscriptModuleDesc::FCodeSection& Section = Module->Code[0];
			ASSERT_THAT(IsTrue(Section.Code.Contains(TEXT("return 7;"))));
			ASSERT_THAT(IsFalse(Section.Code.Contains(
				TEXT("CACHE_EXTERNAL_VALUE"))));

			for (const FAngelscriptProcessedSourceProvenanceRange& Range
				: Section.SourceProvenance)
			{
				ASSERT_THAT(IsTrue(Range.ProcessedCodeUnitOffset >= 0));
				ASSERT_THAT(IsTrue(Range.ProcessedCodeUnitLength > 0));
				ASSERT_THAT(IsTrue(
					Range.ProcessedCodeUnitOffset <= Section.Code.Len()));
				ASSERT_THAT(IsTrue(
					Range.ProcessedCodeUnitLength
						<= Section.Code.Len()
							- Range.ProcessedCodeUnitOffset));
			}

			const FAngelscriptProcessedSourceProvenanceRange* const Helper =
				FindGeneratedRange(
					Section,
					EAngelscriptGeneratedSourceKind::StaticClassHelper);
			ASSERT_THAT(IsNotNull(Helper));
			if (Helper == nullptr)
			{
				return;
			}
			ASSERT_THAT(IsTrue(Section.Code.Mid(
				Helper->ProcessedCodeUnitOffset,
				Helper->ProcessedCodeUnitLength).Contains(
					TEXT("__StaticType_FCachePayload"))));
		}
	}
};

#endif
