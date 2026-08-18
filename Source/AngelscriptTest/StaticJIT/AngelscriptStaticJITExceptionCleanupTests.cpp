#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITExceptionCleanupTests,
	"Angelscript.TestModule.StaticJIT.ExceptionCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExceptionCleanupDestroysLaterDeclaredLocalsFirst)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
			void RaiseAfterTwoLocals()
			{
				FString First = "first";
				FString Second = "second";
				throw("StaticJIT exception cleanup order sentinel");
			}
			)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		const FName ModuleName = FName(TEXT("ASStaticJITExceptionCleanupOrder"));
		const FString SourceFilename = TEXT(
			"/Angelscript/Memory/StaticJITExceptionCleanup/RaiseAfterTwoLocals.as");
		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(
				&Engine,
				ModuleName,
				SourceFilename,
				ScriptSource),
			TEXT("Exception-cleanup fixture should compile")));

		FString GeneratedSource;
		FString GenerationError;
		ASSERT_THAT(IsTrue(
			GenerateStaticJITSourceText(
				&Engine,
				ModuleName,
				GeneratedSource,
				false,
				&GenerationError),
			GenerationError));

		const FString SecondDestructor(TEXT(
			"CastedDestructorPtr((void*)(&v_Second))"));
		const FString FirstDestructor(TEXT(
			"CastedDestructorPtr((void*)(&v_First))"));
		const int32 SecondIndex = GeneratedSource.Find(
			SecondDestructor, ESearchCase::CaseSensitive);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, SecondIndex,
			TEXT("Exception cleanup must emit the later-declared Second destructor")));

		const int32 CleanupHeader = GeneratedSource.Find(
			TEXT("Exception cleanup for variables"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromEnd,
			SecondIndex);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, CleanupHeader,
			TEXT("Second destructor must sit in an exception-cleanup block")));

		const int32 CleanupEnd = GeneratedSource.Find(
			TEXT("\nreturn;"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			SecondIndex);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, CleanupEnd,
			TEXT("Exception-cleanup block must return after destroying live locals")));
		const FString CleanupBlock = GeneratedSource.Mid(
			CleanupHeader, CleanupEnd - CleanupHeader);
		const int32 FirstInBlock = CleanupBlock.Find(
			FirstDestructor, ESearchCase::CaseSensitive);
		const int32 SecondInBlock = CleanupBlock.Find(
			SecondDestructor, ESearchCase::CaseSensitive);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, FirstInBlock,
			TEXT("The two-live exception-cleanup block must also destroy First")));
		ASSERT_THAT(IsTrue(SecondInBlock < FirstInBlock,
			TEXT("Later-declared live locals must be destroyed first during exception cleanup")));
	}
};

#endif
