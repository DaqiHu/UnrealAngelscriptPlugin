#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/Parser/as_parser.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ParserRecovery, "Angelscript.UnitTest.NativeEngine.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	static bool HasErrorAtOffset(const FSessionRun& Run, asUINT ByteOffset)
	{
		asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		for (const asSDiagnosticRecord& Record : Consumer.GetRecords())
		{
			if (Record.Severity == AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error
				&& (Record.PrimaryRange.End.ByteOffset == ByteOffset
					|| (Record.PrimaryRange.Begin.ByteOffset <= ByteOffset
						&& ByteOffset < Record.PrimaryRange.End.ByteOffset)))
			{
				return true;
			}
		}
		return false;
	}

	TEST_METHOD(MalformedFKeepsFollowingG)
	{
		const FString Source = TEXT("int F(){ return 1 } int G(){ return 7; }");
		FSessionRun Run({{TEXT("RecoverFollow.as"), Source}});
		asCParser Parser(Run.Session->GetSema());
		(void)Parser;
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		ASSERT_THAT(IsNotNull(Run.Function(TEXT("G"))));
		ASSERT_THAT(IsFalse(Run.Session->AnalyzeBodies(1)));
		const int32 One = Source.Find(TEXT("1"));
		ASSERT_THAT(IsTrue(One != INDEX_NONE));
		ASSERT_THAT(IsTrue(HasErrorAtOffset(Run, static_cast<asUINT>(One + 1))));
	}

	TEST_METHOD(IncompleteEofIsDistinctFromMidSourceBadToken)
	{
		FSessionRun Eof({{TEXT("RecoverEof.as"), TEXT("int G(){ return 7")}});
		asCParser EofParser(Eof.Session->GetSema());
		(void)EofParser;
		const bool bEofClosed = Eof.Session->CloseDeclarationBarrier();
		asCCollectingDiagnosticConsumer EofConsumer;
		Eof.Diagnostics.EmitTo(EofConsumer);

		const FString Mid = TEXT("int F(){ return 1 } int G(){ return 7; }");
		FSessionRun MidRun({{TEXT("RecoverMid.as"), Mid}});
		asCParser MidParser(MidRun.Session->GetSema());
		(void)MidParser;
		ASSERT_THAT(IsTrue(MidRun.ResolveDeclarations()));
		ASSERT_THAT(IsNotNull(MidRun.Function(TEXT("G"))));
		ASSERT_THAT(IsFalse(MidRun.Session->AnalyzeBodies(1)));
		asCCollectingDiagnosticConsumer MidConsumer;
		MidRun.Diagnostics.EmitTo(MidConsumer);
		ASSERT_THAT(IsTrue(MidConsumer.GetRecords().Num() > 0));
		const bool bEofHasError = EofConsumer.GetRecords().Num() > 0 || !bEofClosed;
		ASSERT_THAT(IsTrue(bEofHasError));
		if (bEofClosed && EofConsumer.GetRecords().Num() > 0 && MidConsumer.GetRecords().Num() > 0)
		{
			ASSERT_THAT(IsTrue(EofConsumer.GetRecords()[0].DiagnosticID != MidConsumer.GetRecords()[0].DiagnosticID));
		}
		else
		{
			ASSERT_THAT(IsTrue(!bEofClosed));
		}
	}
};

#endif
