#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/as_source_provenance.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS \
	&& AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotSourceProvenanceDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.SourceProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CanonicalSourceKindsRemainExplicitWithoutProcessedRelabelling)
	{
		asSSourceSpan SourceSpan;
		SourceSpan.section = "Generated.processed.as";
		SourceSpan.sourceOffset = 91;
		SourceSpan.sourceLength = 17;
		SourceSpan.row = 12;
		SourceSpan.column = 5;
		SourceSpan.hasAuthoredOrigin = true;
		SourceSpan.authoredOrigin.section = "Authored.as";
		SourceSpan.authoredOrigin.sourceOffset = 41;
		SourceSpan.authoredOrigin.sourceLength = 9;
		SourceSpan.authoredOrigin.row = 7;
		SourceSpan.authoredOrigin.column = 3;
		SourceSpan.hasGeneratedOrigin = true;
		SourceSpan.generatedOrigin.kind =
			asEGeneratedSourceKind::LiteralAsset;
		SourceSpan.generatedOrigin.generatorName =
			"LiteralAsset:InventoryIcon";
		SourceSpan.generatedOrigin.hasAuthoredAnchor = true;
		SourceSpan.generatedOrigin.authoredAnchor.section =
			"GeneratedByDeclaration.as";
		SourceSpan.generatedOrigin.authoredAnchor.sourceOffset = 52;
		SourceSpan.generatedOrigin.authoredAnchor.sourceLength = 24;
		SourceSpan.generatedOrigin.authoredAnchor.row = 8;
		SourceSpan.generatedOrigin.authoredAnchor.column = 1;

		const FAngelscriptStaticJITTypedCallDiagnostic Diagnostic =
			FAngelscriptTypedASTJIT::
				BuildSourceProvenanceDiagnosticForTesting(SourceSpan);
		ASSERT_THAT(AreEqual(FString(TEXT("Generated.processed.as")),
			Diagnostic.ProcessedSource.Section));
		ASSERT_THAT(AreEqual(12u, Diagnostic.ProcessedSource.Row));
		ASSERT_THAT(AreEqual(5u, Diagnostic.ProcessedSource.Column));
		ASSERT_THAT(IsTrue(Diagnostic.AuthoredSource.IsSet()));
		ASSERT_THAT(IsTrue(Diagnostic.GeneratedSource.IsSet()));
		ASSERT_THAT(AreEqual(FString(TEXT("Authored.as")),
			Diagnostic.AuthoredSource->Section));
		ASSERT_THAT(AreEqual(7u, Diagnostic.AuthoredSource->Row));
		ASSERT_THAT(AreEqual(FString(TEXT("GeneratedByDeclaration.as")),
			Diagnostic.GeneratedSource->Section));
		ASSERT_THAT(AreEqual(8u, Diagnostic.GeneratedSource->Row));

		asSSourceSpan ProcessedOnly;
		ProcessedOnly.section = "OnlyProcessed.as";
		ProcessedOnly.sourceOffset = 24;
		ProcessedOnly.sourceLength = 6;
		ProcessedOnly.row = 4;
		ProcessedOnly.column = 2;
		const FAngelscriptStaticJITTypedCallDiagnostic ProcessedDiagnostic =
			FAngelscriptTypedASTJIT::
				BuildSourceProvenanceDiagnosticForTesting(ProcessedOnly);
		ASSERT_THAT(AreEqual(FString(TEXT("OnlyProcessed.as")),
			ProcessedDiagnostic.ProcessedSource.Section));
		ASSERT_THAT(IsFalse(ProcessedDiagnostic.AuthoredSource.IsSet()));
		ASSERT_THAT(IsFalse(ProcessedDiagnostic.GeneratedSource.IsSet()));

		asSSourceSpan GeneratedWithoutAnchor = ProcessedOnly;
		GeneratedWithoutAnchor.hasGeneratedOrigin = true;
		GeneratedWithoutAnchor.generatedOrigin.kind =
			asEGeneratedSourceKind::SubsystemAccessor;
		GeneratedWithoutAnchor.generatedOrigin.generatorName =
			"SubsystemAccessor:UInventorySubsystem";
		const FAngelscriptStaticJITTypedCallDiagnostic FallbackDiagnostic =
			FAngelscriptTypedASTJIT::
				BuildSourceProvenanceDiagnosticForTesting(
					GeneratedWithoutAnchor);
		ASSERT_THAT(AreEqual(FString(TEXT("OnlyProcessed.as")),
			FallbackDiagnostic.ProcessedSource.Section));
		ASSERT_THAT(IsFalse(FallbackDiagnostic.AuthoredSource.IsSet()));
		ASSERT_THAT(IsFalse(FallbackDiagnostic.GeneratedSource.IsSet()));
	}
};

#endif
