#include "CQTest.h"
#include "NativeEngine/NativeEngineTestSupport.h"
#include "frontend/Parser/as_parser.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(ParserContracts, "Angelscript.UnitTest.NativeEngine.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	using FSessionRun = AngelscriptNativeEngineTest::FFrontendSessionRun;

	TEST_METHOD(ValidDeclarationCollectsFunctionG)
	{
		const FString Source = TEXT("int G(){ return 7; }");
		FSessionRun Run({{TEXT("Contracts.as"), Source}});
		asCParser Parser(Run.Session->GetSema());
		(void)Parser;
		ASSERT_THAT(IsTrue(Run.ResolveDeclarations()));
		auto* G = Run.Function(TEXT("G"));
		ASSERT_THAT(IsNotNull(G));
		ASSERT_THAT(IsTrue(Run.Session->AnalyzeBodies(1)));
		ASSERT_THAT(IsNotNull(G->GetBody()));
		asCCollectingDiagnosticConsumer Consumer;
		Run.Diagnostics.EmitTo(Consumer);
		for (const asSDiagnosticRecord& Record : Consumer.GetRecords())
		{
			ASSERT_THAT(IsTrue(Record.Severity != AS_NAMESPACE_QUALIFIER asEDiagnosticSeverity::Error));
		}
	}
};

#endif
