#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace TypedSemanticIRSourceProvenanceVerifierTests
{
	static asSTypedSemanticSourceSpan MakeProcessedSpan()
	{
		asSTypedSemanticSourceSpan Span;
		Span.section = "/Angelscript/Game/SourceProvenance.as";
		Span.sourceOffset = 24;
		Span.sourceLength = 8;
		Span.row = 3;
		Span.column = 5;
		return Span;
	}

	static asSTypedSemanticSourceAnchor MakeAnchor(const char* Section)
	{
		asSTypedSemanticSourceAnchor Anchor;
		Anchor.section = Section;
		Anchor.sourceOffset = 7;
		Anchor.sourceLength = 19;
		Anchor.row = 2;
		Anchor.column = 1;
		return Anchor;
	}

	static asCTypedSemanticFunction MakeFunction(
		const asSTypedSemanticSourceSpan& Span)
	{
		asCTypedSemanticFunction Function;
		Function.declaration = "int SourceProvenanceValue()";

		asSTypedSemanticExpression Literal;
		Literal.kind = asETypedSemanticExpressionKind::Literal;
		Literal.type = asCDataType::CreatePrimitive(ttInt, false);
		Literal.literal = "17";
		Literal.span = Span;
		const asTypedSemanticExpressionId LiteralId =
			Function.AddExpression(Literal);

		asSTypedSemanticCleanupPlan EmptyCleanup;
		Function.header.universalCleanupPlan =
			Function.AddCleanupPlan(EmptyCleanup);
		Function.header.cleanupPlanState =
			asETypedSemanticCleanupPlanState::VerifiedEmpty;
		Function.header.cleanupPlanCoversAllTransfers = true;

		asSTypedSemanticStatement Return;
		Return.kind = asETypedSemanticStatementKind::Return;
		Return.span = Span;
		Return.safePointRole = asETypedSemanticSafePointRole::Return;
		Return.valueExpression = LiteralId;
		Return.cleanupPlan = Function.header.universalCleanupPlan;
		Return.exitedScopeCount = 1;
		const asTypedSemanticStatementId ReturnId =
			Function.AddStatement(Return);

		asSTypedSemanticStatement Root;
		Root.kind = asETypedSemanticStatementKind::Block;
		Root.span = Span;
		Root.children.PushLast(ReturnId);
		Function.rootStatement = Function.AddStatement(Root);
		return Function;
	}
}

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRSourceProvenanceVerifierTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.SourceProvenance.Verifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ValidOriginsRemainExplicitAndDiagnosticsPreferHonestCoordinates)
	{
		using namespace TypedSemanticIRSourceProvenanceVerifierTests;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SOURCE-PROVENANCE-VERIFIER",
			AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		asSTypedSemanticSourceSpan Authored = MakeProcessedSpan();
		Authored.hasAuthoredOrigin = true;
		Authored.authoredOrigin = MakeAnchor(
			"/Angelscript/Game/AuthoredSource.as");
		asCTypedSemanticFunction AuthoredFunction = MakeFunction(Authored);
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(
			AuthoredFunction).IsValid()));

		const asSTypedSemanticDiagnosticSource AuthoredDiagnostic =
			SelectTypedSemanticDiagnosticSource(Authored);
		ASSERT_THAT(IsTrue(AuthoredDiagnostic.kind
			== asETypedSemanticDiagnosticSourceKind::Authored));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Game/AuthoredSource.as")),
			FString(UTF8_TO_TCHAR(
				AuthoredDiagnostic.source.section.AddressOf()))));

		asSTypedSemanticSourceSpan Generated = MakeProcessedSpan();
		Generated.hasGeneratedOrigin = true;
		Generated.generatedOrigin.kind =
			asETypedSemanticGeneratedSourceKind::LiteralAsset;
		Generated.generatedOrigin.generatorName = "LiteralAsset:Inventory";
		Generated.generatedOrigin.hasAuthoredAnchor = true;
		Generated.generatedOrigin.authoredAnchor = MakeAnchor(
			"/Angelscript/Game/LiteralAssetSource.as");
		asCTypedSemanticFunction GeneratedFunction = MakeFunction(Generated);
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(
			GeneratedFunction).IsValid()));

		const asSTypedSemanticDiagnosticSource GeneratedDiagnostic =
			SelectTypedSemanticDiagnosticSource(Generated);
		ASSERT_THAT(IsTrue(GeneratedDiagnostic.kind
			== asETypedSemanticDiagnosticSourceKind::GeneratedAnchor));
		ASSERT_THAT(IsTrue(GeneratedDiagnostic.generatedKind
			== asETypedSemanticGeneratedSourceKind::LiteralAsset));
		ASSERT_THAT(AreEqual(FString(TEXT("LiteralAsset:Inventory")),
			FString(UTF8_TO_TCHAR(
				GeneratedDiagnostic.generatorName.AddressOf()))));

		Generated.generatedOrigin.hasAuthoredAnchor = false;
		Generated.generatedOrigin.authoredAnchor = {};
		const asSTypedSemanticDiagnosticSource ProcessedFallback =
			SelectTypedSemanticDiagnosticSource(Generated);
		ASSERT_THAT(IsTrue(ProcessedFallback.kind
			== asETypedSemanticDiagnosticSourceKind::Processed));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Game/SourceProvenance.as")),
			FString(UTF8_TO_TCHAR(
				ProcessedFallback.source.section.AddressOf()))));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(GeneratedFunction).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT(
			"generated=LiteralAsset generator=LiteralAsset:Inventory"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("processed="))));
	}

	TEST_METHOD(MalformedOptionalTuplesFailClosedWithoutRelabellingProcessedOffsets)
	{
		using namespace TypedSemanticIRSourceProvenanceVerifierTests;

		struct FCase
		{
			const TCHAR* Label;
			TFunction<void(asSTypedSemanticSourceSpan&)> Mutate;
		};
		const TArray<FCase> Cases = {
			{TEXT("AuthoredMissingSection"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.hasAuthoredOrigin = true;
					Span.authoredOrigin.sourceLength = 1;
				}},
			{TEXT("AuthoredPayloadWithoutPresence"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.authoredOrigin = MakeAnchor("authored.as");
				}},
			{TEXT("GeneratedMissingKind"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.hasGeneratedOrigin = true;
					Span.generatedOrigin.generatorName = "MissingKind";
				}},
			{TEXT("GeneratedMissingName"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.hasGeneratedOrigin = true;
					Span.generatedOrigin.kind =
						asETypedSemanticGeneratedSourceKind::SubsystemAccessor;
				}},
			{TEXT("GeneratedInvalidAnchor"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.hasGeneratedOrigin = true;
					Span.generatedOrigin.kind =
						asETypedSemanticGeneratedSourceKind::SubsystemAccessor;
					Span.generatedOrigin.generatorName = "Subsystem:Inventory";
					Span.generatedOrigin.hasAuthoredAnchor = true;
				}},
			{TEXT("GeneratedPayloadWithoutPresence"),
				[](asSTypedSemanticSourceSpan& Span)
				{
					Span.generatedOrigin.kind =
						asETypedSemanticGeneratedSourceKind::ClassHelper;
					Span.generatedOrigin.generatorName = "Class:Inventory";
				}},
		};

		for (const FCase& Case : Cases)
		{
			asSTypedSemanticSourceSpan Span = MakeProcessedSpan();
			Case.Mutate(Span);
			const asSTypedSemanticVerificationResult Result =
				VerifyTypedSemanticFunction(MakeFunction(Span));
			ASSERT_THAT(IsTrue(Result.error
				== asETypedSemanticVerificationError::InvalidSourceProvenance,
				Case.Label));
			ASSERT_THAT(AreEqual(24u, Result.span.sourceOffset,
				TEXT("The verifier must report the processed offset honestly")));
		}
	}
};

#endif
