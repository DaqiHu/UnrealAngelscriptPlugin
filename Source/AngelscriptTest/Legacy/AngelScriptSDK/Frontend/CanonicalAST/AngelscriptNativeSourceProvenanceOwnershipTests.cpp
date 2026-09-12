#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "frontend/Basic/as_source_provenance.h"
#include "source/as_scriptcode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalSourceProvenanceOwnershipTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.SourceProvenanceOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ScriptCodePublishesNeutralProvenanceAPI)
	{
		using FSetRanges = bool (asCScriptCode::*)(
			const asSSourceProvenanceRange*, asUINT);
		using FApplySpan = void (asCScriptCode::*)(asSSourceSpan&) const;

		const FSetRanges SetRanges = &asCScriptCode::SetSourceProvenanceRanges;
		const FApplySpan ApplySpan = &asCScriptCode::ApplySourceProvenance;
		ASSERT_THAT(IsTrue(SetRanges != nullptr));
		ASSERT_THAT(IsTrue(ApplySpan != nullptr));
	}

	TEST_METHOD(ScriptCodeAppliesNeutralAuthoredAndGeneratedOrigins)
	{
		asCScriptCode Source;
		const char* const Text = "int GeneratedValue = 42;";
		ASSERT_THAT(AreEqual(asSUCCESS, Source.SetCode(
			"/Angelscript/Game/Processed.as", Text, true)));

		asSSourceProvenanceRange Range;
		Range.processedOffset = 4;
		Range.processedLength = 14;
		Range.hasAuthoredOrigin = true;
		Range.authoredOrigin.section = "/Angelscript/Game/Authored.as";
		Range.authoredOrigin.sourceOffset = 10;
		Range.authoredOrigin.sourceLength = 14;
		Range.authoredOrigin.row = 3;
		Range.authoredOrigin.column = 5;
		Range.hasGeneratedOrigin = true;
		Range.generatedOrigin.kind = asEGeneratedSourceKind::SubsystemAccessor;
		Range.generatedOrigin.generatorName = "USourceFixture";
		Range.generatedOrigin.hasAuthoredAnchor = true;
		Range.generatedOrigin.authoredAnchor = Range.authoredOrigin;

		ASSERT_THAT(IsTrue(Source.SetSourceProvenanceRanges(&Range, 1),
			TEXT("ScriptCode must accept neutral provenance ranges")));

		asSSourceSpan Span;
		Span.section = "/Angelscript/Game/Processed.as";
		Span.sourceOffset = 4;
		Span.sourceLength = 14;
		Span.row = 1;
		Span.column = 5;
		Source.ApplySourceProvenance(Span);

		ASSERT_THAT(IsTrue(Span.hasAuthoredOrigin,
			TEXT("authored provenance must survive HIR-independent ScriptCode storage")));
		ASSERT_THAT(IsTrue(Span.authoredOrigin.section == "/Angelscript/Game/Authored.as"));
		ASSERT_THAT(AreEqual((asUINT)10, Span.authoredOrigin.sourceOffset));
		ASSERT_THAT(IsTrue(Span.hasGeneratedOrigin,
			TEXT("generated provenance must survive HIR-independent ScriptCode storage")));
		ASSERT_THAT(IsTrue(
			Span.generatedOrigin.kind == asEGeneratedSourceKind::SubsystemAccessor));
		ASSERT_THAT(IsTrue(Span.generatedOrigin.generatorName == "USourceFixture"));
		ASSERT_THAT(IsTrue(Span.generatedOrigin.hasAuthoredAnchor));
	}

	TEST_METHOD(NeutralRangesKeepExistingValidationContract)
	{
		asCScriptCode Source;
		ASSERT_THAT(AreEqual(asSUCCESS, Source.SetCode(
			"/Angelscript/Game/Processed.as", "0123456789", true)));

		asSSourceProvenanceRange Empty;
		ASSERT_THAT(IsFalse(Source.SetSourceProvenanceRanges(&Empty, 1),
			TEXT("zero-length provenance must remain invalid")));

		asSSourceProvenanceRange Overlap[2];
		Overlap[0].processedOffset = 1;
		Overlap[0].processedLength = 5;
		Overlap[1].processedOffset = 4;
		Overlap[1].processedLength = 2;
		ASSERT_THAT(IsFalse(Source.SetSourceProvenanceRanges(Overlap, 2),
			TEXT("overlapping provenance must remain invalid")));
	}
};

#endif
