#include "CQTest.h"

#include "unreal/AngelscriptSource.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(DelegatePreprocess, "Angelscript.UnitTest.NativeEngine.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoPreprocessorWrapper)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/DelegatePreprocess/Declare.as"),
			TEXT("DECLARE_DELEGATE_OneParam(FOnDone, int);\n")));
		ASSERT_THAT(IsTrue(Preprocessor.Preprocess()));
		ASSERT_THAT(AreEqual(1, Preprocessor.Files.Num()));
		const FString& Processed = Preprocessor.Files[0].ProcessedCode;
		ASSERT_THAT(IsFalse(Processed.Contains(TEXT("struct FOnDone"))));
		ASSERT_THAT(IsFalse(Processed.Contains(TEXT("_FScriptDelegate _Inner"))));
		ASSERT_THAT(IsTrue(Processed.Contains(TEXT("DECLARE_DELEGATE_OneParam"))));
		ASSERT_THAT(AreEqual(0, Preprocessor.Files[0].Delegates.Num()));
		ASSERT_THAT(AreEqual(0, Preprocessor.Files[0].GeneratedCode.Num()));
	}

	TEST_METHOD(KeywordNotWrapped)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/DelegatePreprocess/Keyword.as"),
			TEXT("delegate int FOnDone();\n")));
		ASSERT_THAT(IsTrue(Preprocessor.Preprocess()));
		ASSERT_THAT(AreEqual(1, Preprocessor.Files.Num()));
		const FString& Processed = Preprocessor.Files[0].ProcessedCode;
		ASSERT_THAT(IsFalse(Processed.Contains(TEXT("_Inner"))));
		ASSERT_THAT(IsFalse(Processed.Contains(TEXT("struct FOnDone"))));
		ASSERT_THAT(AreEqual(0, Preprocessor.Files[0].Delegates.Num()));
		for (const FAngelscriptPreprocessor::FGeneratedCode& Generated : Preprocessor.Files[0].GeneratedCode)
		{
			ASSERT_THAT(IsFalse(Generated.GeneratorName.Contains(TEXT("DelegateHelper"))));
		}
	}

	TEST_METHOD(MultiLineDeclareOneChunk)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/DelegatePreprocess/Multiline.as"),
			TEXT("DECLARE_DYNAMIC_DELEGATE_TwoParams(\n")
			TEXT("    FDynTwo,\n")
			TEXT("    int, First,\n")
			TEXT("    int, Second);\n")));
		ASSERT_THAT(IsTrue(Preprocessor.Preprocess()));
		ASSERT_THAT(AreEqual(1, Preprocessor.Files.Num()));
		int32 GlobalChunks = 0;
		bool bSawDeclare = false;
		for (int32 ChunkIndex = 0; ChunkIndex < Preprocessor.Files[0].ChunkedCode.Num(); ++ChunkIndex)
		{
			const FAngelscriptPreprocessor::FChunk& Chunk = Preprocessor.Files[0].ChunkedCode[ChunkIndex];
			if (Chunk.Type == FAngelscriptPreprocessor::EChunkType::Global)
			{
				++GlobalChunks;
				if (Chunk.Content.Contains(TEXT("DECLARE_DYNAMIC_DELEGATE_TwoParams"))
					&& Chunk.Content.Contains(TEXT("FDynTwo"))
					&& Chunk.Content.Contains(TEXT("Second")))
				{
					bSawDeclare = true;
				}
			}
		}
		ASSERT_THAT(AreEqual(1, GlobalChunks));
		ASSERT_THAT(IsTrue(bSawDeclare));
	}
};

#endif
