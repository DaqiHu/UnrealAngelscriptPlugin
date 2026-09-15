#include "CQTest.h"
#include "../NativeEngineTestSupport.h"
#include "../Compiler/NativeSourceExecutionTestSupport.h"

#include "as_builder.h"
#include "as_bytecode_emitter.h"
#include "as_compile_output.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DiagnosticsBuilder,
	"Angelscript.UnitTest.NativeEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	static const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* FindGroup(
		const AS_NAMESPACE_QUALIFIER asCDiagnosticResult& Result,
		asUINT DiagnosticID)
	{
		for (const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup& Group : Result.GetGroups())
		{
			if (Group.Primary.DiagnosticID == DiagnosticID)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	TEST_METHOD(SingleRootThroughCompileOutput)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("builder-diagnostics");
		Options.Diagnostics.bSuppressDisplay = true;
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCCompileOutput> Output;
		{
			AS_NAMESPACE_QUALIFIER asCBuilder Builder(
				AngelscriptNativeEngineTest::MakeSourceList(
					TEXT("Typo.as"),
					TEXT("int F(){ int Count=1; return Cout; }")),
				Options);
			ASSERT_THAT(IsFalse(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
			Output = Builder.TakeCompileOutput();
			ASSERT_THAT(IsNotNull(Output.Get()));
		}
		const TSharedRef<const AS_NAMESPACE_QUALIFIER asCDiagnosticResult, ESPMode::ThreadSafe> Result =
			Output->GetDiagnosticResult();
		ASSERT_THAT(IsTrue(Result->HasLanguageFailure()));
		ASSERT_THAT(AreEqual(0, Result->GetVisibleGroups().Num()));
		const AS_NAMESPACE_QUALIFIER asSDiagnosticGroup* Group = FindGroup(
			*Result,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::UnresolvedReference));
		ASSERT_THAT(IsNotNull(Group));
		ASSERT_THAT(AreEqual(1, Group->Notes.Num()));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::DidYouMean),
			Group->Notes[0].DiagnosticID));
		ASSERT_THAT(AreEqual(1, Group->Fixes.Num()));
		ASSERT_THAT(IsNull(FindGroup(
			*Result,
			static_cast<asUINT>(AS_NAMESPACE_QUALIFIER asEDiagnosticCatalogID::BuilderStage))));
		ASSERT_THAT(IsNull(FindGroup(*Result, 3003)));
		ASSERT_THAT(AreEqual(0, Output->GetDiagnostics().Num()));
	}

	TEST_METHOD(EmissionDetailReachesConsumer)
	{
		FSourceExecutionInput Input(TEXT("int Run() { return 42; }"));
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(Input.Sources, Input.Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		AS_NAMESPACE_QUALIFIER asSByteCodeEmissionOptions EmissionOptions;
		EmissionOptions.MaxFrameDWords = 1;
		TUniquePtr<AS_NAMESPACE_QUALIFIER asCByteCodeImage> Image;
		const AS_NAMESPACE_QUALIFIER asSByteCodeEmissionResult Emitted = AS_NAMESPACE_QUALIFIER asEmitByteCodeImage(
			Builder.GetCompilationSession(),
			Builder.GetModuleDefinitionSet(),
			EmissionOptions,
			Image);
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEByteCodeEmissionStatus::ResourceLimit,
			Emitted.EmissionStatus));
		AS_NAMESPACE_QUALIFIER asSBuilderStageResult Ingested;
		AS_NAMESPACE_QUALIFIER asIngestByteCodeEmissionResult(Ingested, Emitted);
		ASSERT_THAT(IsTrue(Ingested.bHasEmission));
		ASSERT_THAT(AreEqual(Emitted.EmissionStatus, Ingested.EmissionStatus));
		ASSERT_THAT(IsTrue(Ingested.EmissionFailure.FunctionKey == Emitted.Failure.FunctionKey));
		ASSERT_THAT(AreEqual(Emitted.Failure.NodeKind, Ingested.EmissionFailure.NodeKind));
		ASSERT_THAT(AreEqual(Emitted.Failure.StableSourceKey, Ingested.EmissionFailure.StableSourceKey));
		ASSERT_THAT(AreEqual(Emitted.Failure.StableFragmentKey, Ingested.EmissionFailure.StableFragmentKey));
		ASSERT_THAT(IsTrue(Ingested.EmissionFailure.Diagnostics.Num() > 0));

		FSourceExecutionInput SuccessInput(TEXT("int Run() { return 42; }"));
		AS_NAMESPACE_QUALIFIER asCBuilder Success(SuccessInput.Sources, SuccessInput.Options);
		ASSERT_THAT(IsTrue(Success.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted)));
		const AS_NAMESPACE_QUALIFIER asSBuilderStageResult* Stage =
			Success.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::ByteCodeEmitted);
		ASSERT_THAT(IsNotNull(Stage));
		ASSERT_THAT(IsTrue(Stage->bHasEmission));
		ASSERT_THAT(AreEqual(
			AS_NAMESPACE_QUALIFIER asEByteCodeEmissionStatus::Succeeded,
			Stage->EmissionStatus));
	}

	TEST_METHOD(NonpoisoningRejectedRequest)
	{
		AS_NAMESPACE_QUALIFIER asSBuilderOptions Options;
		Options.StableScope = TEXT("builder-diagnostics");
		AS_NAMESPACE_QUALIFIER asCBuilder Builder(
			AngelscriptNativeEngineTest::MakeSourceList(TEXT("Ok.as"), TEXT("int G(){ return 2; }")),
			Options);
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady, Builder.GetStage()));
		ASSERT_THAT(IsFalse(Builder.RunStage(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		ASSERT_THAT(AreEqual(AS_NAMESPACE_QUALIFIER asEBuilderStage::SourceReady, Builder.GetStage()));
		ASSERT_THAT(IsNull(Builder.GetStageResult(AS_NAMESPACE_QUALIFIER asEBuilderStage::BodiesAnalyzed)));
		ASSERT_THAT(IsTrue(Builder.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNotNull(Builder.GetModuleDefinitionSet()));

		AS_NAMESPACE_QUALIFIER asCBuilder Hidden(
			AngelscriptNativeEngineTest::MakeSourceList(
				TEXT("Typo.as"),
				TEXT("int F(){ int Count=1; return Cout; }")),
			Options);
		ASSERT_THAT(IsFalse(Hidden.RunThrough(AS_NAMESPACE_QUALIFIER asEBuilderStage::DefinitionsFrozen)));
		ASSERT_THAT(IsNull(Hidden.GetModuleDefinitionSet()));
		ASSERT_THAT(IsTrue(Hidden.CaptureDiagnosticResult()->HasLanguageFailure()));
	}
};

#endif
