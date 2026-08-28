#include "AngelscriptTestMacros.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeBuilderTestSupport.h"

#include "CQTest.h"
#include <cstring>

#include "StartAngelscriptHeaders.h"
#include "source/as_ast_context.h"
#include "source/as_builder.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_sema.h"
#include "source/as_source_manager.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCanonicalSourceManagerTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.CanonicalAST.SourceManager",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LogicalSectionIdentityAndLineMapping)
	{
		asCSourceManager Manager;
		const char* Source = "int A = 1;\nint B = 2;\n";
		const asASTFileID File = Manager.AddSection(
			"/Angelscript/Game/Script/Sample.as",
			asAST_SOURCE_AUTHORED,
			Source,
			std::strlen(Source),
			0);
		ASSERT_THAT(IsTrue(File != 0, TEXT("authored section should receive a snapshot-local FileID")));
		ASSERT_THAT(AreEqual(asAST_SOURCE_AUTHORED, Manager.GetOrigin(File), TEXT("origin should stay authored")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(Manager.GetLogicalKey(File), "/Angelscript/Game/Script/Sample.as") == 0,
			TEXT("logical key should be independent from FileID")));

		asSSourceLineColumn LineColumn;
		const asCSourceLocation Loc = Manager.MakeLocation(File, 11);
		ASSERT_THAT(IsTrue(Manager.GetLineColumn(Loc, LineColumn), TEXT("line mapping should succeed")));
		ASSERT_THAT(AreEqual(2, LineColumn.row, TEXT("second line should be row 2")));
	}

	TEST_METHOD(ProcessedGeneratedAndInvalidRange)
	{
		asCSourceManager Manager;
		const char* Authored = "int Value = 1;";
		const char* Generated = "void _default() {}";
		const asASTFileID AuthoredId = Manager.AddSection("sample.as", asAST_SOURCE_AUTHORED, Authored, std::strlen(Authored));
		const asASTFileID ProcessedId = Manager.AddSection("sample.as", asAST_SOURCE_PROCESSED, Authored, std::strlen(Authored));
		const asASTFileID GeneratedId = Manager.AddSection("<generated:default:sample>", asAST_SOURCE_GENERATED, Generated, std::strlen(Generated));
		ASSERT_THAT(IsTrue(AuthoredId != ProcessedId, TEXT("same logical key with different origin should get distinct FileIDs")));
		ASSERT_THAT(IsTrue(GeneratedId != 0, TEXT("generated provenance should be a first-class section")));

		const asCSourceRange Bad = Manager.MakeRange(AuthoredId, 8, 1);
		ASSERT_THAT(IsTrue(!Bad.IsValid(), TEXT("reversed offsets should be rejected")));
		const asCSourceRange PastEnd = Manager.MakeRange(AuthoredId, 0, 1000);
		ASSERT_THAT(IsTrue(!PastEnd.begin.IsValid() || !Manager.IsValidRange(PastEnd),
			TEXT("range past the section buffer should be rejected")));
		ASSERT_THAT(IsTrue(Manager.AddSection(nullptr, asAST_SOURCE_AUTHORED, Authored, std::strlen(Authored)) == 0,
			TEXT("null logical key should be rejected")));
	}

	TEST_METHOD(DeterministicRemapAssignsNewLocalIds)
	{
		asCSourceManager First;
		asCSourceManager Second;
		const char* Source = "class C {}";
		const asASTFileID FirstId = First.AddSection("mod.as", asAST_SOURCE_AUTHORED, Source, std::strlen(Source));
		const asASTFileID SecondId = Second.RemapLogical("mod.as", asAST_SOURCE_AUTHORED, Source, std::strlen(Source));
		ASSERT_THAT(IsTrue(FirstId != 0 && SecondId != 0, TEXT("both managers should allocate FileIDs")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(First.GetLogicalKey(FirstId), Second.GetLogicalKey(SecondId)) == 0,
			TEXT("logical path should survive remap")));
		ASSERT_THAT(AreEqual(SecondId, Second.RemapLogical("mod.as", asAST_SOURCE_AUTHORED, Source, std::strlen(Source)),
			TEXT("second remap of the same logical key should reuse the snapshot-local FileID")));
	}

	TEST_METHOD(RemapRejectsChangedContentForExistingLogicalSource)
	{
		asCSourceManager Manager;
		const char* Original = "int First;\n";
		const char* Changed = "int Second;\nint Third;\n";
		const asASTFileID OriginalId = Manager.RemapLogical(
			"/Angelscript/Game/Script/Remapped.as",
			asAST_SOURCE_PROCESSED,
			Original,
			std::strlen(Original),
			3);
		ASSERT_THAT(IsTrue(OriginalId != 0, TEXT("initial remap should allocate a snapshot-local source")));

		const asASTFileID ReoffsetId = Manager.RemapLogical(
			"/Angelscript/Game/Script/Remapped.as",
			asAST_SOURCE_PROCESSED,
			Original,
			std::strlen(Original),
			7);
		ASSERT_THAT(AreEqual((asASTFileID)0, ReoffsetId,
			TEXT("a logical source with a changed line offset must not silently reuse stale coordinates")));

		const asASTFileID ChangedId = Manager.RemapLogical(
			"/Angelscript/Game/Script/Remapped.as",
			asAST_SOURCE_PROCESSED,
			Changed,
			std::strlen(Changed),
			3);
		ASSERT_THAT(AreEqual((asASTFileID)0, ChangedId,
			TEXT("a logical source with changed bytes must not silently reuse stale coordinates")));
		ASSERT_THAT(AreEqual((asUINT)1, Manager.GetSectionCount(),
			TEXT("failed remap must not add a second ambiguous section")));
		ASSERT_THAT(AreEqual(3, Manager.GetLineOffset(OriginalId),
			TEXT("failed remap must preserve the original line mapping")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(Manager.GetBuffer(OriginalId), Original) == 0,
			TEXT("failed remap must preserve the original source bytes")));
	}

	TEST_METHOD(NeutralGeneratedProvenanceIsOwnedAndResolvedByCanonicalSourceManager)
	{
		asCSourceManager Manager;
		const char* const Processed =
			"int AuthoredValue = 1;\n"
			"int __GeneratedSubsystemValue = 2;\n";
		const char* const Generated = std::strstr(
			Processed, "__GeneratedSubsystemValue");
		ASSERT_THAT(IsNotNull(Generated,
			TEXT("the provenance fixture must contain its generated token")));
		if (Generated == nullptr)
		{
			return;
		}

		asSSourceProvenanceRange Provenance;
		Provenance.processedOffset = static_cast<asUINT>(Generated - Processed);
		Provenance.processedLength = static_cast<asUINT>(
			std::strlen("__GeneratedSubsystemValue"));
		Provenance.hasAuthoredOrigin = true;
		Provenance.authoredOrigin.section =
			"/Angelscript/Game/AuthoredSource.as";
		Provenance.authoredOrigin.sourceOffset = 19;
		Provenance.authoredOrigin.sourceLength = 11;
		Provenance.authoredOrigin.row = 4;
		Provenance.authoredOrigin.column = 2;
		Provenance.hasGeneratedOrigin = true;
		Provenance.generatedOrigin.kind =
			asEGeneratedSourceKind::SubsystemAccessor;
		Provenance.generatedOrigin.generatorName = "USourceManagerFixture";
		Provenance.generatedOrigin.hasAuthoredAnchor = true;
		Provenance.generatedOrigin.authoredAnchor = Provenance.authoredOrigin;

		const asASTFileID File = Manager.AddSection(
			"/Angelscript/Game/ProcessedSource.as",
			asAST_SOURCE_PROCESSED,
			Processed,
			std::strlen(Processed),
			7,
			&Provenance,
			1);
		ASSERT_THAT(IsTrue(File != 0,
			TEXT("Canonical SourceManager must accept validated neutral provenance")));
		if (File == 0)
		{
			return;
		}

		// Prove snapshot ownership rather than aliasing the caller's strings.
		Provenance.generatedOrigin.generatorName = "CallerMutatedAfterInstall";
		asSSourceSpan Span;
		ASSERT_THAT(IsTrue(Manager.ResolveSourceSpan(
			Manager.MakeRange(
				File,
				static_cast<asUINT>(Generated - Processed),
				static_cast<asUINT>(Generated - Processed)
					+ static_cast<asUINT>(std::strlen("__GeneratedSubsystemValue"))),
			Span),
			TEXT("Canonical SourceManager must resolve a processed AST range")));
		ASSERT_THAT(IsTrue(Span.section == "/Angelscript/Game/ProcessedSource.as"));
		ASSERT_THAT(AreEqual(static_cast<asUINT>(Generated - Processed),
			Span.sourceOffset));
		ASSERT_THAT(AreEqual(9u, Span.row,
			TEXT("processed coordinates must retain the SourceManager line offset")));
		ASSERT_THAT(IsTrue(Span.hasAuthoredOrigin,
			TEXT("authored origin must be available without TypedSemantic HIR")));
		ASSERT_THAT(IsTrue(
			Span.authoredOrigin.section == "/Angelscript/Game/AuthoredSource.as"));
		ASSERT_THAT(IsTrue(Span.hasGeneratedOrigin,
			TEXT("generated origin must be available without TypedSemantic HIR")));
		ASSERT_THAT(IsTrue(Span.generatedOrigin.kind
			== asEGeneratedSourceKind::SubsystemAccessor));
		ASSERT_THAT(IsTrue(
			Span.generatedOrigin.generatorName == "USourceManagerFixture",
			TEXT("SourceManager must own a copy of installed provenance")));
		ASSERT_THAT(IsTrue(Span.generatedOrigin.hasAuthoredAnchor));
	}

	TEST_METHOD(RemapRejectsChangedProvenanceForExistingLogicalSource)
	{
		asCSourceManager Manager;
		const char* const Processed = "int __Generated = 1;";
		asSSourceProvenanceRange Original;
		Original.processedOffset = 4;
		Original.processedLength = 11;
		Original.hasGeneratedOrigin = true;
		Original.generatedOrigin.kind = asEGeneratedSourceKind::LiteralAsset;
		Original.generatedOrigin.generatorName = "LiteralAsset:Original";

		const asASTFileID File = Manager.RemapLogical(
			"/Angelscript/Game/ProcessedRemap.as",
			asAST_SOURCE_PROCESSED,
			Processed,
			std::strlen(Processed),
			0,
			&Original,
			1);
		ASSERT_THAT(IsTrue(File != 0));

		asSSourceProvenanceRange Changed = Original;
		Changed.generatedOrigin.generatorName = "LiteralAsset:Changed";
		ASSERT_THAT(AreEqual((asASTFileID)0, Manager.RemapLogical(
			"/Angelscript/Game/ProcessedRemap.as",
			asAST_SOURCE_PROCESSED,
			Processed,
			std::strlen(Processed),
			0,
			&Changed,
			1),
			TEXT("same bytes with changed provenance must not reuse stale diagnostic identity")));
		ASSERT_THAT(AreEqual((asUINT)1, Manager.GetSectionCount(),
			TEXT("a rejected provenance remap must not add an ambiguous section")));
	}

	TEST_METHOD(ParserSemaCopiesScriptCodeProvenanceIntoCanonicalSourceSession)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "CanonicalSourceProvenance");
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Canonical provenance test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCScriptCode Source;
		const char* const SourceText = "int GeneratedValue = 42;";
		ASSERT_THAT(AreEqual(asSUCCESS, Source.SetCode(
			"/Angelscript/Game/ProcessedGenerated.as", SourceText, true)));

		asSSourceProvenanceRange Range;
		Range.processedOffset = 4;
		Range.processedLength = 14;
		Range.hasAuthoredOrigin = true;
		Range.authoredOrigin.section = "/Angelscript/Game/AuthoredGenerated.as";
		Range.authoredOrigin.sourceOffset = 25;
		Range.authoredOrigin.sourceLength = 8;
		Range.authoredOrigin.row = 6;
		Range.authoredOrigin.column = 3;
		Range.hasGeneratedOrigin = true;
		Range.generatedOrigin.kind = asEGeneratedSourceKind::LiteralAsset;
		Range.generatedOrigin.generatorName = "LiteralAsset:GeneratedValue";
		Range.generatedOrigin.hasAuthoredAnchor = true;
		Range.generatedOrigin.authoredAnchor = Range.authoredOrigin;
		ASSERT_THAT(IsTrue(Source.SetSourceProvenanceRanges(&Range, 1)));

		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Source),
			TEXT("the native parser may coexist while Canonical Sema captures source facts")));

		const asCSourceManager& Sources = Context.GetSourceManager();
		ASSERT_THAT(AreEqual((asUINT)1, Sources.GetSectionCount()));
		asSSourceSpan Span;
		ASSERT_THAT(IsTrue(Sources.ResolveSourceSpan(
			Sources.MakeRange(1, 4, 18), Span),
			TEXT("Canonical source session must resolve ScriptCode provenance")));
		ASSERT_THAT(IsTrue(Span.hasAuthoredOrigin));
		ASSERT_THAT(IsTrue(
			Span.authoredOrigin.section == "/Angelscript/Game/AuthoredGenerated.as"));
		ASSERT_THAT(IsTrue(Span.hasGeneratedOrigin));
		ASSERT_THAT(IsTrue(
			Span.generatedOrigin.kind == asEGeneratedSourceKind::LiteralAsset));
		ASSERT_THAT(IsTrue(
			Span.generatedOrigin.generatorName == "LiteralAsset:GeneratedValue"));
	}

	TEST_METHOD(ParserSemaRejectsChangedContentForSameLogicalSection)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "SourceIdentity");
		ASSERT_THAT(IsNotNull(Module, TEXT("source identity test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);

		asCScriptCode First;
		ASSERT_THAT(AreEqual(0, First.SetCode(
			"SharedLogicalSection.as", "int First() { return 1; }", true)));
		First.lineOffset = 3;
		asCParser FirstParser(&Builder);
		FirstParser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, FirstParser.ParseScript(&First),
			TEXT("the first logical section must parse")));
		const asUINT DeclCountBeforeConflict = Context.GetDeclCount();
		ASSERT_THAT(AreEqual((asUINT)1, Context.GetSourceManager().GetSectionCount(),
			TEXT("the first parsed section must own one source record")));

		asCScriptCode Changed;
		ASSERT_THAT(AreEqual(0, Changed.SetCode(
			"SharedLogicalSection.as", "int Other() { return 2; }", true)));
		Changed.lineOffset = 7;
		asCParser ChangedParser(&Builder);
		ChangedParser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, ChangedParser.ParseScript(&Changed),
			TEXT("legacy syntax parsing remains independent from canonical source capture")));

		bool bSawSourceIdentityDiagnostic = false;
		for (asUINT Index = 0; Index < Sema.GetDiagnostics().GetLength(); ++Index)
		{
			if (Sema.GetDiagnostics()[Index].Equals("source-remap-mismatch"))
			{
				bSawSourceIdentityDiagnostic = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bSawSourceIdentityDiagnostic,
			TEXT("canonical capture must reject a same-name source whose bytes or line mapping changed")));
		ASSERT_THAT(AreEqual(DeclCountBeforeConflict, Context.GetDeclCount(),
			TEXT("a rejected source identity must not add AST declarations with stale coordinates")));
		ASSERT_THAT(AreEqual((asUINT)1, Context.GetSourceManager().GetSectionCount(),
			TEXT("a rejected source identity must not create an ambiguous second source record")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
			Context.GetSourceManager().GetBuffer(1), "int First() { return 1; }") == 0,
			TEXT("the original source bytes remain authoritative after conflict rejection")));
	}

	TEST_METHOD(CanonicalSemaDiagnosticRangeMapsThroughOwnedSourceSession)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "CanonicalSemaDiagnosticSource");
		ASSERT_THAT(IsNotNull(Module, TEXT("canonical Sema diagnostic test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;
		asCASTContext Context;
		asCSema Sema(ScriptEngine, Context);
		asCScriptCode Source;
		const char* const SourceText =
			"int F()\n"
			"{\n"
			"\treturn MissingValue;\n"
			"}\n";
		ASSERT_THAT(AreEqual(0, Source.SetCode(
			"/Angelscript/Game/Script/CanonicalSemaDiagnosticSource.as",
			SourceText,
			true)));
		Source.lineOffset = 20;

		asCParser Parser(&Builder);
		Parser.SetSema(&Sema);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Source),
			TEXT("syntax parsing should complete while Canonical Sema records the unresolved identifier")));
		ASSERT_THAT(AreEqual(Sema.GetDiagnostics().GetLength(), Sema.GetDiagnosticRanges().GetLength(),
			TEXT("every Canonical Sema diagnostic must carry a parallel source-range fact")));

		asUINT DiagnosticIndex = asUINT(-1);
		for (asUINT Index = 0; Index < Sema.GetDiagnostics().GetLength(); ++Index)
		{
			if (Sema.GetDiagnostics()[Index].StartsWith("unresolved-identifier:MissingValue"))
			{
				DiagnosticIndex = Index;
				break;
			}
		}
		ASSERT_THAT(IsTrue(DiagnosticIndex != asUINT(-1),
			TEXT("Canonical Sema must publish the unresolved identifier diagnostic")));
		if (DiagnosticIndex == asUINT(-1))
		{
			return;
		}

		const asCSourceRange& DiagnosticRange = Sema.GetDiagnosticRanges()[DiagnosticIndex];
		const asCSourceManager& Sources = Context.GetSourceManager();
		ASSERT_THAT(IsTrue(Sources.IsValidRange(DiagnosticRange),
			TEXT("Canonical Sema diagnostic range must belong to its AST SourceManager")));
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
			Sources.GetLogicalKey(DiagnosticRange.begin.fileID),
			"/Angelscript/Game/Script/CanonicalSemaDiagnosticSource.as") == 0,
			TEXT("Canonical Sema diagnostic must retain the parser logical source key")));

		asSSourceLineColumn LineColumn = {};
		ASSERT_THAT(IsTrue(Sources.GetLineColumn(DiagnosticRange.begin, LineColumn),
			TEXT("Canonical Sema diagnostic must map through the owned source session")));
		ASSERT_THAT(AreEqual(23, LineColumn.row,
			TEXT("Canonical Sema diagnostic must apply the original line offset")));
		ASSERT_THAT(AreEqual(9, LineColumn.column,
			TEXT("Canonical Sema diagnostic must retain the unresolved token column")));
	}

	TEST_METHOD(ParserDiagnosticsPublishSourceSessionCoordinates)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "DiagnosticSource");
		ASSERT_THAT(IsNotNull(Module, TEXT("diagnostic source test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(ScriptEngine, Module);
		asCScriptCode Source;
		ASSERT_THAT(AreEqual(0, Source.SetCode(
			"/Angelscript/Game/Script/DiagnosticSource.as",
			"void F(\n{\n}\n",
			true)));
		Source.lineOffset = 12;

		asCParser Parser(&Builder);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&Source) < 0,
			TEXT("the parser fixture must report a real syntax diagnostic")));

		const asCSourceManager& DiagnosticSources = Builder.GetDiagnosticSourceManager();
		ASSERT_THAT(AreEqual((asUINT)1, DiagnosticSources.GetSectionCount(),
			TEXT("parser diagnostics must publish their source section into the builder source session")));
		const asASTFileID File = DiagnosticSources.GetFileIDByIndex(0);
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
			DiagnosticSources.GetLogicalKey(File), "/Angelscript/Game/Script/DiagnosticSource.as") == 0,
			TEXT("diagnostic source session must preserve the parser logical key")));
		ASSERT_THAT(AreEqual(12, DiagnosticSources.GetLineOffset(File),
			TEXT("diagnostic source session must preserve the parser line offset")));

		asSSourceLineColumn LineColumn = {};
		ASSERT_THAT(IsTrue(DiagnosticSources.GetLineColumn(
			DiagnosticSources.MakeLocation(File, 8), LineColumn),
			TEXT("the invalid token offset must be locatable through the diagnostic source session")));
		ASSERT_THAT(AreEqual(14, LineColumn.row,
			TEXT("the diagnostic source session must apply the original line offset")));
		ASSERT_THAT(AreEqual(1, LineColumn.column,
			TEXT("the diagnostic source session must preserve the invalid token column")));

		bool bSawMappedSyntaxError = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Message : Engine.GetMessages().Entries)
		{
			bSawMappedSyntaxError |= Message.Type == asMSGTYPE_ERROR
				&& Message.Section == TEXT("/Angelscript/Game/Script/DiagnosticSource.as")
				&& Message.Row == 14
				&& Message.Column == 1;
		}
		ASSERT_THAT(IsTrue(bSawMappedSyntaxError,
			TEXT("the parser syntax error must use the source-session logical section and mapped coordinates")));
	}

	TEST_METHOD(BuilderSemanticDiagnosticsPublishSourceSessionCoordinates)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "BuilderDiagnosticSource");
		ASSERT_THAT(IsNotNull(Module, TEXT("builder diagnostic source test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		const char* const Source =
			"enum OverflowEnum\n"
			"{\n"
			"\tTooLarge = 128\n"
			"}\n";
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"/Angelscript/Game/Script/BuilderDiagnosticSource.as",
			Source,
			std::strlen(Source),
			7),
			TEXT("the semantic diagnostic fixture must add its source section")));

		asCBuilder* const Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("adding a source section must create a builder")));
		if (Builder == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(0, Builder->BuildParallelParseScripts(),
			TEXT("the semantic diagnostic fixture must parse before semantic registration")));
		ASSERT_THAT(AreEqual(0, Builder->BuildGenerateTypes(),
			TEXT("the semantic diagnostic fixture must register its enum type")));
		ASSERT_THAT(IsTrue(Builder->BuildGenerateFunctions() < 0,
			TEXT("the out-of-range enum value must report a Builder semantic diagnostic")));

		const asCSourceManager& DiagnosticSources = Builder->GetDiagnosticSourceManager();
		ASSERT_THAT(AreEqual((asUINT)1, DiagnosticSources.GetSectionCount(),
			TEXT("Builder semantic diagnostics must publish their source section into the diagnostic source session")));
		if (DiagnosticSources.GetSectionCount() != 1)
		{
			return;
		}

		const asASTFileID File = DiagnosticSources.GetFileIDByIndex(0);
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
			DiagnosticSources.GetLogicalKey(File), "/Angelscript/Game/Script/BuilderDiagnosticSource.as") == 0,
			TEXT("Builder semantic diagnostics must preserve the logical source key")));
		ASSERT_THAT(AreEqual(7, DiagnosticSources.GetLineOffset(File),
			TEXT("Builder semantic diagnostics must preserve the source line offset")));

		const char* const EnumValue = std::strstr(Source, "TooLarge");
		ASSERT_THAT(IsNotNull(EnumValue, TEXT("the fixture must contain the overflowing enum value token")));
		if (EnumValue == nullptr)
		{
			return;
		}
		asSSourceLineColumn LineColumn = {};
		ASSERT_THAT(IsTrue(DiagnosticSources.GetLineColumn(
			DiagnosticSources.MakeLocation(File, static_cast<asUINT>(EnumValue - Source)), LineColumn),
			TEXT("the Builder semantic diagnostic token must be locatable through the source session")));
		ASSERT_THAT(AreEqual(10, LineColumn.row,
			TEXT("the Builder semantic diagnostic must retain the original line offset")));
		ASSERT_THAT(AreEqual(2, LineColumn.column,
			TEXT("the Builder semantic diagnostic must retain the original token column")));

		bool bSawMappedSemanticError = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Message : Engine.GetMessages().Entries)
		{
			bSawMappedSemanticError |= Message.Type == asMSGTYPE_ERROR
				&& Message.Section == TEXT("/Angelscript/Game/Script/BuilderDiagnosticSource.as")
				&& Message.Row == 10
				&& Message.Column == 2
				&& Message.Message.Contains(TEXT("Enum value too large"));
		}
		ASSERT_THAT(IsTrue(bSawMappedSemanticError,
			TEXT("the Builder semantic error must preserve its legacy message and source-session coordinates")));
	}

	TEST_METHOD(CompilerSemanticDiagnosticsPublishSourceSessionCoordinates)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		asCModule* const Module = AngelscriptBuilderTestSupport::CreateBuilderModule(
			ScriptEngine, "CompilerDiagnosticSource");
		ASSERT_THAT(IsNotNull(Module, TEXT("compiler diagnostic source test needs a builder module")));
		if (Module == nullptr)
		{
			return;
		}

		const char* const Source =
			"int Entry()\n"
			"{\n"
			"\tGhostType Value;\n"
			"\treturn 0;\n"
			"}\n";
		ASSERT_THAT(AreEqual(0, Module->AddScriptSection(
			"/Angelscript/Game/Script/CompilerDiagnosticSource.as",
			Source,
			std::strlen(Source),
			6),
			TEXT("the compiler diagnostic fixture must add its source section")));

		asCBuilder* const Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder, TEXT("adding a source section must create a builder")));
		if (Builder == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::RunBuilderPipelineThroughLayout(
			*TestRunner, *Builder, Module),
			TEXT("the compiler diagnostic fixture must build through layout before body compilation")));
		ASSERT_THAT(IsTrue(Builder->BuildCompileCode() < 0,
			TEXT("the unresolved type must report a compiler semantic diagnostic")));

		const asCSourceManager& DiagnosticSources = Builder->GetDiagnosticSourceManager();
		ASSERT_THAT(AreEqual((asUINT)1, DiagnosticSources.GetSectionCount(),
			TEXT("compiler semantic diagnostics must publish their source section into the diagnostic source session")));
		if (DiagnosticSources.GetSectionCount() != 1)
		{
			return;
		}

		const asASTFileID File = DiagnosticSources.GetFileIDByIndex(0);
		ASSERT_THAT(IsTrue(FCStringAnsi::Strcmp(
			DiagnosticSources.GetLogicalKey(File), "/Angelscript/Game/Script/CompilerDiagnosticSource.as") == 0,
			TEXT("compiler semantic diagnostics must preserve the logical source key")));
		ASSERT_THAT(AreEqual(6, DiagnosticSources.GetLineOffset(File),
			TEXT("compiler semantic diagnostics must preserve the source line offset")));

		const char* const UnknownType = std::strstr(Source, "GhostType");
		ASSERT_THAT(IsNotNull(UnknownType, TEXT("the fixture must contain the unresolved type token")));
		if (UnknownType == nullptr)
		{
			return;
		}
		asSSourceLineColumn LineColumn = {};
		ASSERT_THAT(IsTrue(DiagnosticSources.GetLineColumn(
			DiagnosticSources.MakeLocation(File, static_cast<asUINT>(UnknownType - Source)), LineColumn),
			TEXT("the compiler semantic diagnostic token must be locatable through the source session")));
		ASSERT_THAT(AreEqual(9, LineColumn.row,
			TEXT("the compiler semantic diagnostic must retain the original line offset")));
		ASSERT_THAT(AreEqual(2, LineColumn.column,
			TEXT("the compiler semantic diagnostic must retain the original token column")));

		bool bSawMappedCompilerError = false;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Message : Engine.GetMessages().Entries)
		{
			bSawMappedCompilerError |= Message.Type == asMSGTYPE_ERROR
				&& Message.Section == TEXT("/Angelscript/Game/Script/CompilerDiagnosticSource.as")
				&& Message.Row == 9
				&& Message.Column == 2
				&& Message.Message.Contains(TEXT("GhostType"));
		}
		ASSERT_THAT(IsTrue(bSawMappedCompilerError,
			TEXT("the compiler semantic error must preserve its legacy message and source-session coordinates")));
	}
};

#endif
